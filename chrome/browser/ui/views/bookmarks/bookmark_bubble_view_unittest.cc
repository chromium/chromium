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
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/price_tracking/mock_shopping_list_ui_tab_helper.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "chrome/browser/ui/views/commerce/price_tracking_view.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/views/chrome_test_widget.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
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

class BookmarkBubbleViewTest : public BrowserWithTestWindowTest {
 public:
  // The test executes the UI code for displaying a window that should be
  // executed on the UI thread. The test also hits the networking code that
  // fails without the IO thread. We pass the REAL_IO_THREAD option to run UI
  // and IO tasks on separate threads.
  BookmarkBubbleViewTest()
      : BrowserWithTestWindowTest(
            content::BrowserTaskEnvironment::REAL_IO_THREAD) {
#if !BUILDFLAG(IS_FUCHSIA)
    test_features_.InitAndEnableFeature(commerce::kShoppingList);
#endif  // !BUILDFLAG(IS_FUCHSIA)
  }

  BookmarkBubbleViewTest(const BookmarkBubbleViewTest&) = delete;
  BookmarkBubbleViewTest& operator=(const BookmarkBubbleViewTest&) = delete;

  // testing::Test:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    anchor_widget_ =
        views::UniqueWidgetPtr(std::make_unique<ChromeTestWidget>());
    views::Widget::InitParams widget_params;
    widget_params.context = GetContext();
    anchor_widget_->Init(std::move(widget_params));

    bookmark_model_ = BookmarkModelFactory::GetForBrowserContext(profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);

    bookmarks::AddIfNotBookmarked(bookmark_model_, GURL(kTestBookmarkURL),
                                  std::u16string());

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

    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories = {
        {BookmarkModelFactory::GetInstance(),
         BookmarkModelFactory::GetDefaultFactory()},
        {commerce::ShoppingServiceFactory::GetInstance(),
         base::BindRepeating([](content::BrowserContext* context) {
           return commerce::MockShoppingService::Build();
         })}};
    IdentityTestEnvironmentProfileAdaptor::
        AppendIdentityTestEnvironmentFactories(&factories);
    return factories;
  }

  raw_ptr<BookmarkModel> GetBookmarkModel() { return bookmark_model_; }

 protected:
  // Creates a bookmark bubble view.
  void CreateBubbleView() {
    // Create a fake anchor view for the bubble.
    BookmarkBubbleView::ShowBubble(
        anchor_widget_->GetContentsView(),
        browser()->tab_strip_model()->GetActiveWebContents(), nullptr, nullptr,
        browser(), GURL(kTestBookmarkURL), true);
  }

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

  void SimulateProductImageIsAvailable(bool with_valid_image) {
    MockShoppingListUiTabHelper::CreateForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
    mock_tab_helper_ = static_cast<MockShoppingListUiTabHelper*>(
        MockShoppingListUiTabHelper::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents()));
    EXPECT_CALL(*mock_tab_helper_, GetProductImage);
    if (with_valid_image) {
      const gfx::Image image = mock_tab_helper_->GetValidProductImage();
      ON_CALL(*mock_tab_helper_, GetProductImage)
          .WillByDefault(
              testing::ReturnRef(mock_tab_helper_->GetValidProductImage()));
    } else {
      ON_CALL(*mock_tab_helper_, GetProductImage)
          .WillByDefault(
              testing::ReturnRef(mock_tab_helper_->GetInvalidProductImage()));
    }
  }

 private:
  views::UniqueWidgetPtr anchor_widget_;
  base::test::ScopedFeatureList test_features_;
  raw_ptr<BookmarkModel> bookmark_model_;
  raw_ptr<MockShoppingListUiTabHelper> mock_tab_helper_;
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

#if !BUILDFLAG(IS_FUCHSIA)
// Verifies that the price tracking view is displayed for trackable product.
TEST_F(BookmarkBubbleViewTest, PriceTrackingViewIsVisible) {
  commerce::MockShoppingService* mock_shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForBrowserContext(profile()));

  SimulateProductImageIsAvailable(/*with_valid_image=*/true);

  commerce::ProductInfo info;
  info.product_cluster_id.emplace(12345L);
  mock_shopping_service->SetIsSubscribedCallbackValue(false);
  mock_shopping_service->SetResponseForGetProductInfoForUrl(info);
  CreateBubbleView();
  // Verify the view is displayed with toggle off.
  auto* price_tracking_view = GetPriceTrackingView();
  EXPECT_TRUE(price_tracking_view);
  EXPECT_FALSE(price_tracking_view->IsToggleOn());
}

TEST_F(BookmarkBubbleViewTest, PriceTrackingViewIsHidden) {
  commerce::MockShoppingService* mock_shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForBrowserContext(profile()));
  mock_shopping_service->SetResponseForGetProductInfoForUrl(absl::nullopt);

  CreateBubbleView();
  auto* price_tracking_view = GetPriceTrackingView();
  EXPECT_FALSE(price_tracking_view);
}

TEST_F(BookmarkBubbleViewTest, PriceTrackingViewIsHidden_ImageNotAvailable) {
  commerce::MockShoppingService* mock_shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForBrowserContext(profile()));
  mock_shopping_service->SetResponseForGetProductInfoForUrl(
      commerce::ProductInfo());
  SimulateProductImageIsAvailable(/*with_valid_image=*/false);

  CreateBubbleView();
  // Verify the view is hidden.
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
  commerce::ProductInfo info;
  info.product_cluster_id.emplace(12345L);
  mock_shopping_service->SetResponseForGetProductInfoForUrl(info);
  SimulateProductImageIsAvailable(/*with_valid_image=*/true);

  CreateBubbleView();
  auto* price_tracking_view = GetPriceTrackingView();
  EXPECT_TRUE(price_tracking_view);
  EXPECT_TRUE(price_tracking_view->IsToggleOn());
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

#if !BUILDFLAG(IS_FUCHSIA)
class PriceTrackingViewFeatureFlagTest
    : public BookmarkBubbleViewTest,
      public testing::WithParamInterface<bool> {
 public:
  PriceTrackingViewFeatureFlagTest() {
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
  base::test::ScopedFeatureList test_features_;
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

  MockShoppingListUiTabHelper::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  auto* mock_tab_helper_ = static_cast<MockShoppingListUiTabHelper*>(
      MockShoppingListUiTabHelper::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents()));
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

#endif  // !BUILDFLAG(IS_FUCHSIA)
