// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/mock_commerce_ui_tab_helper.h"
#include "chrome/browser/ui/signin/bubble_signin_promo_delegate.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/commerce/price_tracking_view.h"
#include "chrome/browser/ui/views/commerce/shopping_collection_iph_view.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/views/chrome_test_widget.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/test_sync_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/unique_widget_ptr.h"
using bookmarks::BookmarkModel;

namespace {
const char kTestBookmarkURL[] = "http://www.google.com";
} // namespace

class BookmarkBubbleViewTestBase : public BrowserWithTestWindowTest {
 public:
  // The test executes the UI code for displaying a window that should be
  // executed on the UI thread. The test also hits the networking code that
  // fails without the IO thread. We pass the REAL_IO_THREAD option to run UI
  // and IO tasks on separate threads.
  BookmarkBubbleViewTestBase()
      : BrowserWithTestWindowTest(
            content::BrowserTaskEnvironment::REAL_IO_THREAD) {}

  BookmarkBubbleViewTestBase(const BookmarkBubbleViewTestBase&) = delete;
  BookmarkBubbleViewTestBase& operator=(const BookmarkBubbleViewTestBase&) =
      delete;

  // testing::Test:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    anchor_widget_ =
        views::UniqueWidgetPtr(std::make_unique<ChromeTestWidget>());
    views::Widget::InitParams widget_params(
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
    widget_params.context = GetContext();
    anchor_widget_->Init(std::move(widget_params));

    bookmark_model_ = BookmarkModelFactory::GetForBrowserContext(profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);

    bookmark_node_ = bookmarks::AddIfNotBookmarked(
        bookmark_model_, GURL(kTestBookmarkURL), std::u16string());

    AddTab(browser(), GURL(kTestBookmarkURL));
    browser()->tab_strip_model()->ActivateTabAt(0);
  }

  void TearDown() override {
    // Make sure the bubble is destroyed before the profile to avoid a crash.
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        BookmarkBubbleView::bookmark_bubble()->GetWidget());
    BookmarkBubbleView::bookmark_bubble()->GetWidget()->Close();
    destroyed_waiter.Wait();

    anchor_widget_.reset();

    bookmark_node_ = nullptr;

    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
            {TestingProfile::TestingFactory{
                 BookmarkModelFactory::GetInstance(),
                 BookmarkModelFactory::GetDefaultFactory()},
             TestingProfile::TestingFactory{
                 commerce::ShoppingServiceFactory::GetInstance(),
                 base::BindRepeating([](content::BrowserContext* context) {
                   return commerce::MockShoppingService::Build();
                 })},
             // Used by IdentityTestEnvironmentProfileAdaptor.
             TestingProfile::TestingFactory{
                 ChromeSigninClientFactory::GetInstance(),
                 base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                     test_url_loader_factory())},
             // Used by ImageService.
             TestingProfile::TestingFactory{
                 SyncServiceFactory::GetInstance(),
                 base::BindRepeating([](content::BrowserContext*) {
                   return static_cast<std::unique_ptr<KeyedService>>(
                       std::make_unique<syncer::TestSyncService>());
                 })}});
  }

  BookmarkModel* GetBookmarkModel() { return bookmark_model_; }

 protected:
  // Creates a bookmark bubble view.
  void CreateBubbleView(bool already_bookmarked = true) {
    // Create a fake anchor view for the bubble.
    BookmarkBubbleView::ShowBubble(
        anchor_widget_->GetContentsView(),
        browser()->tab_strip_model()->GetActiveWebContents(), nullptr, nullptr,
        browser(), GURL(kTestBookmarkURL), already_bookmarked);
  }

  const bookmarks::BookmarkNode* GetBookmark() { return bookmark_node_; }

  PriceTrackingView* GetPriceTrackingView() {
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForView(
            BookmarkBubbleView::bookmark_bubble()->GetAnchorView());
    views::View* matched_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kPriceTrackingBookmarkViewElementId, context);

    return matched_view ? views::AsViewClass<PriceTrackingView>(matched_view)
                        : nullptr;
  }

  base::test::ScopedFeatureList test_features_;

 private:
  raw_ptr<const bookmarks::BookmarkNode> bookmark_node_;
  views::UniqueWidgetPtr anchor_widget_;
  raw_ptr<BookmarkModel, DanglingUntriaged> bookmark_model_;
  raw_ptr<MockCommerceUiTabHelper, DanglingUntriaged> mock_tab_helper_;
};

class BookmarkBubbleViewTest : public BookmarkBubbleViewTestBase {
 public:
  BookmarkBubbleViewTest() {
    test_features_.InitAndEnableFeature(commerce::kShoppingList);
  }
};

// Verifies that the sync promo is not displayed for a signed in user.
TEST_F(BookmarkBubbleViewTest, SyncPromoSignedIn) {
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile()),
      "fake_username@gmail.com", signin::ConsentLevel::kSync);
  CreateBubbleView();
  EXPECT_FALSE(
      BookmarkBubbleView::bookmark_bubble()->GetFootnoteViewForTesting());
}

// Verifies that the sync promo is displayed for a user that is not signed in.
TEST_F(BookmarkBubbleViewTest, SyncPromoNotSignedIn) {
  CreateBubbleView();
  views::View* footnote =
      BookmarkBubbleView::bookmark_bubble()->GetFootnoteViewForTesting();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_FALSE(footnote);
#else  // !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(footnote);
#endif
}

// Verifies that the price tracking view is displayed for trackable product.
TEST_F(BookmarkBubbleViewTest, PriceTrackingViewIsVisible) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  commerce::MockShoppingService* mock_shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForBrowserContext(profile()));
  mock_shopping_service->SetIsShoppingListEligible(true);

  commerce::ProductInfo info;
  info.product_cluster_id.emplace(12345L);
  mock_shopping_service->SetIsSubscribedCallbackValue(false);
  mock_shopping_service->SetResponseForGetProductInfoForUrl(info);
  CreateBubbleView();
  // Verify the view is displayed with toggle off.
  auto* price_tracking_view = GetPriceTrackingView();
  EXPECT_TRUE(price_tracking_view);
  EXPECT_FALSE(price_tracking_view->IsToggleOn());

  // No Price Tracking UKM tracked for bookmark that is not new.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Shopping_ShoppingAction::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

TEST_F(BookmarkBubbleViewTest, RecordPriceTrackingUkmForNewBookmark) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  commerce::MockShoppingService* mock_shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForBrowserContext(profile()));
  mock_shopping_service->SetIsShoppingListEligible(true);

  commerce::ProductInfo info;
  info.product_cluster_id.emplace(12345L);
  mock_shopping_service->SetIsSubscribedCallbackValue(false);
  mock_shopping_service->SetResponseForGetProductInfoForUrl(info);

  CreateBubbleView(false);

  // Price Tracking UKM tracked for new bookmark.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Shopping_ShoppingAction::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Shopping_ShoppingAction::kPriceTrackedName, 1);
}

TEST_F(BookmarkBubbleViewTest, PriceTrackingViewIsHidden) {
  commerce::MockShoppingService* mock_shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForBrowserContext(profile()));
  mock_shopping_service->SetResponseForGetProductInfoForUrl(std::nullopt);

  CreateBubbleView();
  auto* price_tracking_view = GetPriceTrackingView();
  EXPECT_FALSE(price_tracking_view);
}

// Verifies that the price tracking view is displayed with the correct toggle
// state
TEST_F(BookmarkBubbleViewTest, PriceTrackingViewWithToggleOn) {
  commerce::AddProductBookmark(GetBookmarkModel(), u"title",
                               GURL(kTestBookmarkURL), 0, true);

  commerce::MockShoppingService* mock_shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForBrowserContext(profile()));
  mock_shopping_service->SetIsShoppingListEligible(true);

  commerce::ProductInfo info;
  info.product_cluster_id.emplace(12345L);
  mock_shopping_service->SetResponseForGetProductInfoForUrl(info);
  mock_shopping_service->SetIsSubscribedCallbackValue(true);

  CreateBubbleView();
  auto* price_tracking_view = GetPriceTrackingView();
  EXPECT_TRUE(price_tracking_view);
  EXPECT_TRUE(price_tracking_view->IsToggleOn());
}

class PriceTrackingViewFeatureFlagTest
    : public BookmarkBubbleViewTestBase,
      public testing::WithParamInterface<bool> {
 public:
  PriceTrackingViewFeatureFlagTest() {
    MockCommerceUiTabHelper::ReplaceFactory();
    const bool is_feature_enabled = GetParam();
    if (is_feature_enabled) {
      test_features_.InitAndEnableFeature(commerce::kShoppingList);
    } else {
      test_features_.InitAndDisableFeature(commerce::kShoppingList);
    }
  }
  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    return info.param ? "ShoppingListEnabled" : "ShoppingListDisabled";
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         PriceTrackingViewFeatureFlagTest,
                         testing::Bool(),
                         &PriceTrackingViewFeatureFlagTest::DescribeParams);

TEST_P(PriceTrackingViewFeatureFlagTest, PriceTrackingViewCreation) {
  commerce::MockShoppingService* mock_shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForBrowserContext(profile()));
  commerce::ProductInfo info;
  info.product_cluster_id.emplace(12345L);
  mock_shopping_service->SetResponseForGetProductInfoForUrl(info);

  const bool is_feature_enabled = GetParam();
  mock_shopping_service->SetIsShoppingListEligible(is_feature_enabled);

  auto* mock_tab_helper_ =
      static_cast<MockCommerceUiTabHelper*>(browser()
                                                ->GetActiveTabInterface()
                                                ->GetTabFeatures()
                                                ->commerce_ui_tab_helper());
  const gfx::Image image = mock_tab_helper_->GetValidProductImage();
  ON_CALL(*mock_tab_helper_, GetProductImage)
      .WillByDefault(
          testing::ReturnRef(mock_tab_helper_->GetValidProductImage()));

  CreateBubbleView();

  auto* price_tracking_view = GetPriceTrackingView();

  if (is_feature_enabled) {
    EXPECT_TRUE(price_tracking_view);
  } else {
    EXPECT_FALSE(price_tracking_view);
  }
}

class BookmarkBubbleViewShoppingCollectionTest
    : public BookmarkBubbleViewTestBase {
 public:
  void SetUp() override {
    BookmarkBubbleViewTestBase::SetUp();

    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile()), "test@example.com",
        signin::ConsentLevel::kSync);
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories =
        BookmarkBubbleViewTestBase::GetTestingFactories();

    factories.emplace_back(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(
            &BookmarkBubbleViewShoppingCollectionTest::BuildMockTracker));

    return factories;
  }

  static std::unique_ptr<KeyedService> BuildMockTracker(
      content::BrowserContext* context) {
    auto tracker = std::make_unique<feature_engagement::test::MockTracker>();
    ON_CALL(*tracker, ShouldTriggerHelpUI(testing::Ref(
                          feature_engagement::kIPHShoppingCollectionFeature)))
        .WillByDefault(testing::Return(true));
    return tracker;
  }

  void MoveBookmarkToShoppingCollection() {
    const bookmarks::BookmarkNode* collection =
        commerce::GetShoppingCollectionBookmarkFolder(GetBookmarkModel(), true);

    GetBookmarkModel()->Move(GetBookmark(), collection,
                             collection->children().size());
  }

  void AddProductInfoToBookmark() {
    commerce::AddProductInfoToExistingBookmark(
        GetBookmarkModel(), GetBookmark(), u"product", 12345L);
  }
};

TEST_F(BookmarkBubbleViewShoppingCollectionTest, IPHShown) {
  AddProductInfoToBookmark();
  MoveBookmarkToShoppingCollection();

  CreateBubbleView();

  const ui::ElementContext context =
      views::ElementTrackerViews::GetContextForView(
          BookmarkBubbleView::bookmark_bubble()->GetAnchorView());
  views::View* iph_root =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          commerce::kShoppingCollectionIPHViewId, context);

  // The IPH should be shown in this case.
  EXPECT_TRUE(iph_root);
  EXPECT_TRUE(
      BookmarkBubbleView::bookmark_bubble()->GetFootnoteViewForTesting());
}

TEST_F(BookmarkBubbleViewShoppingCollectionTest, IPHNotShown_NotInCollection) {
  AddProductInfoToBookmark();

  CreateBubbleView();

  const ui::ElementContext context =
      views::ElementTrackerViews::GetContextForView(
          BookmarkBubbleView::bookmark_bubble()->GetAnchorView());
  views::View* iph_root =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          commerce::kShoppingCollectionIPHViewId, context);

  // The IPH should not be shown.
  EXPECT_FALSE(iph_root);
  EXPECT_FALSE(
      BookmarkBubbleView::bookmark_bubble()->GetFootnoteViewForTesting());
}

TEST_F(BookmarkBubbleViewShoppingCollectionTest, IPHNotShown_NotAProduct) {
  MoveBookmarkToShoppingCollection();

  CreateBubbleView();

  const ui::ElementContext context =
      views::ElementTrackerViews::GetContextForView(
          BookmarkBubbleView::bookmark_bubble()->GetAnchorView());
  views::View* iph_root =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          commerce::kShoppingCollectionIPHViewId, context);

  // The IPH should not be shown.
  EXPECT_FALSE(iph_root);
  EXPECT_FALSE(
      BookmarkBubbleView::bookmark_bubble()->GetFootnoteViewForTesting());
}
