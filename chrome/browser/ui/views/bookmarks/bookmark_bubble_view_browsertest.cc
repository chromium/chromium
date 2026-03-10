// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/commerce/mock_commerce_ui_tab_helper.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/commerce/price_tracking_view.h"
#include "chrome/browser/ui/views/commerce/shopping_collection_iph_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"

class BaseBookmarkBubbleViewBrowserTest : public DialogBrowserTest {
 public:
  BaseBookmarkBubbleViewBrowserTest() = default;
  ~BaseBookmarkBubbleViewBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&BaseBookmarkBubbleViewBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    weak_ptr_factory_.GetWeakPtr()));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    commerce::ShoppingServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context) {
          return commerce::MockShoppingService::Build();
        }));
  }

  void ShowUi(const std::string& name) override {
#if !BUILDFLAG(IS_CHROMEOS)
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    signin::MakePrimaryAccountAvailable(identity_manager, "testuser@gtest.com",
                                        signin::ConsentLevel::kSignin);
#endif

    if (name == "bookmark_details_on_trackable_product") {
      commerce::ProductInfo info;
      info.product_cluster_id.emplace(12345L);
      commerce::MockShoppingService* mock_shopping_service =
          static_cast<commerce::MockShoppingService*>(
              commerce::ShoppingServiceFactory::GetForBrowserContext(
                  browser()->profile()));
      mock_shopping_service->SetIsShoppingListEligible(true);
      mock_shopping_service->SetResponseForGetProductInfoForUrl(info);
      mock_shopping_service->SetIsSubscribedCallbackValue(false);
    }

    const GURL url = GURL("https://www.google.com");
    const std::u16string title = u"Title";
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
    bookmarks::AddIfNotBookmarked(bookmark_model, url, title);
    browser()->window()->ShowBookmarkBubble(url, true);

    if (name == "ios_promotion") {
      BookmarkBubbleView::bookmark_bubble()->AcceptDialog();
    }
  }

 protected:
  base::test::ScopedFeatureList test_features_;

 private:
  base::CallbackListSubscription create_services_subscription_;
  base::WeakPtrFactory<BaseBookmarkBubbleViewBrowserTest> weak_ptr_factory_{
      this};
};

class PowerBookmarkBubbleViewBrowserTest
    : public BaseBookmarkBubbleViewBrowserTest {
 public:
  PowerBookmarkBubbleViewBrowserTest() {
    commerce_ui_override_ = MockCommerceUiTabHelper::ReplaceFactory();
    test_features_.InitWithFeatures({commerce::kShoppingList}, {});
  }
  ~PowerBookmarkBubbleViewBrowserTest() override = default;

 private:
  ui::UserDataFactory::ScopedOverride commerce_ui_override_;
};

IN_PROC_BROWSER_TEST_F(PowerBookmarkBubbleViewBrowserTest,
                       InvokeUi_bookmark_details_on_trackable_product) {
  ShowAndVerifyUi();
}

namespace {
const char kTestBookmarkURL[] = "http://www.google.com";
}

class BookmarkBubbleViewMigrationBrowserTest : public InProcessBrowserTest {
 public:
  enum class InitializationMode { kEnableShoppingList, kDefer };

  explicit BookmarkBubbleViewMigrationBrowserTest(
      InitializationMode mode = InitializationMode::kEnableShoppingList) {
    if (mode == InitializationMode::kEnableShoppingList) {
      test_features_.InitAndEnableFeature(commerce::kShoppingList);
    }
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&BookmarkBubbleViewMigrationBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    weak_ptr_factory_.GetWeakPtr()));
  }

  virtual void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) {
    commerce::ShoppingServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context) {
          return commerce::MockShoppingService::Build();
        }));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    bookmark_model_ =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);
    bookmark_node_ = bookmarks::AddIfNotBookmarked(
        bookmark_model_, GURL(kTestBookmarkURL), std::u16string());

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(kTestBookmarkURL)));
  }

  void TearDownOnMainThread() override {
    if (BookmarkBubbleView::bookmark_bubble()) {
      views::test::WidgetDestroyedWaiter destroyed_waiter(
          BookmarkBubbleView::bookmark_bubble()->GetWidget());
      BookmarkBubbleView::bookmark_bubble()->GetWidget()->CloseWithReason(
          views::Widget::ClosedReason::kUnspecified);
      destroyed_waiter.Wait();
    }
    bookmark_model_ = nullptr;
    bookmark_node_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void CreateBubbleView(bool already_bookmarked = true) {
    browser()->window()->ShowBookmarkBubble(GURL(kTestBookmarkURL),
                                            already_bookmarked);
  }

  bookmarks::BookmarkModel* GetBookmarkModel() { return bookmark_model_; }
  const bookmarks::BookmarkNode* GetBookmark() { return bookmark_node_; }

  views::View* GetViewInBookmarkBubble(ui::ElementIdentifier id) {
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForView(
            BookmarkBubbleView::bookmark_bubble()->GetAnchorView());
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
        id, context);
  }

  PriceTrackingView* GetPriceTrackingView() {
    views::View* const matched_view =
        GetViewInBookmarkBubble(kPriceTrackingBookmarkViewElementId);
    return matched_view ? views::AsViewClass<PriceTrackingView>(matched_view)
                        : nullptr;
  }

 protected:
  raw_ptr<const bookmarks::BookmarkNode> bookmark_node_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  base::test::ScopedFeatureList test_features_;

 private:
  base::CallbackListSubscription create_services_subscription_;
  base::WeakPtrFactory<BookmarkBubbleViewMigrationBrowserTest>
      weak_ptr_factory_{this};
};

// Verifies that the sync promo is not displayed for a signed in user.
// TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is
// deleted from the codebase. See ConsentLevel::kSync documentation for
// details.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewMigrationBrowserTest,
                       SyncPromoSignedIn) {
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(browser()->profile()),
      "fake_username@gmail.com", signin::ConsentLevel::kSync);
  CreateBubbleView();
  EXPECT_FALSE(
      BookmarkBubbleView::bookmark_bubble()->GetFootnoteViewForTesting());
}

// Verifies that the sync promo is displayed for a user that is not signed in.
// TODO(crbug.com/491520553): This test is highly brittle in the
// InProcessBrowserTest environment. It relies on injecting a TestSyncService
// before the real IdentityManager and SyncService fully initialize, which
// causes timing and race conditions when attempting to trick the native UI
// layer into showing the "Not Signed In" promo state. See the migration KI for
// details on mocking SyncService.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewMigrationBrowserTest,
                       DISABLED_SyncPromoNotSignedIn) {
  CreateBubbleView();
  views::View* footnote =
      BookmarkBubbleView::bookmark_bubble()->GetFootnoteViewForTesting();
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(footnote);
#else
  EXPECT_TRUE(footnote);
#endif
}

// Verifies that the price tracking view is displayed for trackable product.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewMigrationBrowserTest,
                       PriceTrackingViewIsVisible) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  commerce::MockShoppingService* mock_shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForBrowserContext(
              browser()->profile()));
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

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewMigrationBrowserTest,
                       RecordPriceTrackingUkmForNewBookmark) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  commerce::MockShoppingService* mock_shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForBrowserContext(
              browser()->profile()));
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

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewMigrationBrowserTest,
                       PriceTrackingViewIsHidden) {
  commerce::MockShoppingService* mock_shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForBrowserContext(
              browser()->profile()));
  mock_shopping_service->SetResponseForGetProductInfoForUrl(std::nullopt);

  CreateBubbleView();
  auto* price_tracking_view = GetPriceTrackingView();
  EXPECT_FALSE(price_tracking_view);
}

// Verifies that the price tracking view is displayed with the correct toggle
// state
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewMigrationBrowserTest,
                       PriceTrackingViewWithToggleOn) {
  commerce::AddProductBookmark(GetBookmarkModel(), u"title",
                               GURL(kTestBookmarkURL), 0, true);

  commerce::MockShoppingService* mock_shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForBrowserContext(
              browser()->profile()));
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

class PriceTrackingViewFeatureFlagBrowserTest
    : public BookmarkBubbleViewMigrationBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  PriceTrackingViewFeatureFlagBrowserTest()
      : BookmarkBubbleViewMigrationBrowserTest(InitializationMode::kDefer) {
    commerce_ui_override_ = MockCommerceUiTabHelper::ReplaceFactory();
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

 private:
  ui::UserDataFactory::ScopedOverride commerce_ui_override_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PriceTrackingViewFeatureFlagBrowserTest,
    testing::Bool(),
    &PriceTrackingViewFeatureFlagBrowserTest::DescribeParams);

IN_PROC_BROWSER_TEST_P(PriceTrackingViewFeatureFlagBrowserTest,
                       PriceTrackingViewCreation) {
  commerce::MockShoppingService* mock_shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForBrowserContext(
              browser()->profile()));
  commerce::ProductInfo info;
  info.product_cluster_id.emplace(12345L);
  mock_shopping_service->SetResponseForGetProductInfoForUrl(info);

  const bool is_feature_enabled = GetParam();
  mock_shopping_service->SetIsShoppingListEligible(is_feature_enabled);

  auto* mock_tab_helper =
      static_cast<MockCommerceUiTabHelper*>(browser()
                                                ->GetActiveTabInterface()
                                                ->GetTabFeatures()
                                                ->commerce_ui_tab_helper());
  ON_CALL(*mock_tab_helper, GetProductImage)
      .WillByDefault(
          testing::ReturnRef(mock_tab_helper->GetValidProductImage()));

  CreateBubbleView();

  auto* price_tracking_view = GetPriceTrackingView();

  if (is_feature_enabled) {
    EXPECT_TRUE(price_tracking_view);
  } else {
    EXPECT_FALSE(price_tracking_view);
  }
}

class BookmarkBubbleViewShoppingCollectionBrowserTest
    : public BookmarkBubbleViewMigrationBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  BookmarkBubbleViewShoppingCollectionBrowserTest()
      : BookmarkBubbleViewMigrationBrowserTest(InitializationMode::kDefer) {
    if (GetParam()) {
      test_features_.InitWithFeatures(
          {commerce::kShoppingList, syncer::kReplaceSyncPromosWithSignInPromos},
          {});
    } else {
      test_features_.InitWithFeatures(
          {commerce::kShoppingList},
          {syncer::kReplaceSyncPromosWithSignInPromos});
    }
  }

  void SetUpOnMainThread() override {
    BookmarkBubbleViewMigrationBrowserTest::SetUpOnMainThread();

    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(browser()->profile()),
        "test@example.com",
        GetParam() ? signin::ConsentLevel::kSignin
                   : signin::ConsentLevel::kSync);
    if (GetParam()) {
      GetBookmarkModel()->CreateAccountPermanentFolders();
    }
  }

  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    BookmarkBubbleViewMigrationBrowserTest::OnWillCreateBrowserContextServices(
        context);
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&BookmarkBubbleViewShoppingCollectionBrowserTest::
                                BuildMockTracker));
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

IN_PROC_BROWSER_TEST_P(BookmarkBubbleViewShoppingCollectionBrowserTest,
                       IPHShown) {
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

IN_PROC_BROWSER_TEST_P(BookmarkBubbleViewShoppingCollectionBrowserTest,
                       IPHNotShown_NotInCollection) {
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

IN_PROC_BROWSER_TEST_P(BookmarkBubbleViewShoppingCollectionBrowserTest,
                       IPHNotShown_NotAProduct) {
  MoveBookmarkToShoppingCollection();

  CreateBubbleView();

  const ui::ElementContext context =
      views::ElementTrackerViews::GetContextForView(
          BookmarkBubbleView::bookmark_bubble()->GetAnchorView());
  views::View* const iph_root =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          commerce::kShoppingCollectionIPHViewId, context);

  // The IPH should not be shown.
  EXPECT_FALSE(iph_root);
  EXPECT_FALSE(
      BookmarkBubbleView::bookmark_bubble()->GetFootnoteViewForTesting());
}

#if BUILDFLAG(IS_CHROMEOS)
// The feature `switches::kSyncEnableBookmarksInTransportMode`, prerequisite of
// `syncer::kReplaceSyncPromosWithSignInPromos`, is disabled for ChromeOS.
INSTANTIATE_TEST_SUITE_P(All,
                         BookmarkBubbleViewShoppingCollectionBrowserTest,
                         testing::Values(false));
#else
INSTANTIATE_TEST_SUITE_P(All,
                         BookmarkBubbleViewShoppingCollectionBrowserTest,
                         testing::Bool());
#endif

class BookmarkBubbleViewWithAccountBookmarksBrowserTest
    : public BookmarkBubbleViewMigrationBrowserTest {
 public:
  BookmarkBubbleViewWithAccountBookmarksBrowserTest()
      : BookmarkBubbleViewMigrationBrowserTest(InitializationMode::kDefer) {
    test_features_.InitWithFeatures(
        {commerce::kShoppingList,
         switches::kSyncEnableBookmarksInTransportMode},
        {});
  }
};

// Verifies that the bookmark bubble correctly instantiates a combobox that
// separates account bookmarks and local bookmarks with headers. It also
// verifies that RecentlyUsedFoldersComboModel correctly reports those as
// "title" items and that ComboboxMenuModel correctly translates that to a
// TYPE_TITLE item that can be rendered differently when the popup menu is
// displayed within the browser UI environment.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewWithAccountBookmarksBrowserTest,
                       ComboboxUsesTitlesForHeaders) {
  GetBookmarkModel()->CreateAccountPermanentFolders();
  CreateBubbleView();

  views::View* folder_field = GetViewInBookmarkBubble(kBookmarkFolderFieldId);
  ASSERT_TRUE(folder_field);
  views::Combobox* combobox = views::AsViewClass<views::Combobox>(folder_field);
  ASSERT_TRUE(combobox);

  EXPECT_EQ(ui::MenuModel::TYPE_TITLE,
            combobox->menu_model_for_testing()->GetTypeAt(0));
}
