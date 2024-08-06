// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/commerce/mock_commerce_ui_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/commerce/price_tracking_bubble_dialog_view.h"
#include "chrome/browser/ui/views/commerce/price_tracking_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/test_utils.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace {
const char kTrackableUrl[] = "http://google.com";
const char kNonTrackableUrl[] = "about:blank";

}  // namespace

class PriceTrackingIconViewIntegrationTest : public TestWithBrowserView {
 public:
  PriceTrackingIconViewIntegrationTest() {
    MockCommerceUiTabHelper::ReplaceFactory();
  }

  PriceTrackingIconViewIntegrationTest(
      const PriceTrackingIconViewIntegrationTest&) = delete;
  PriceTrackingIconViewIntegrationTest& operator=(
      const PriceTrackingIconViewIntegrationTest&) = delete;

  ~PriceTrackingIconViewIntegrationTest() override = default;

  void SetUp() override {
    test_features_.InitAndEnableFeature(commerce::kShoppingList);
    TestWithBrowserView::SetUp();
    AddTab(browser(), GURL(kNonTrackableUrl));
    mock_tab_helper_ =
        static_cast<MockCommerceUiTabHelper*>(browser()
                                                  ->GetActiveTabInterface()
                                                  ->GetTabFeatures()
                                                  ->commerce_ui_tab_helper());
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories =
        TestWithBrowserView::GetTestingFactories();
    factories.emplace_back(BookmarkModelFactory::GetInstance(),
                           BookmarkModelFactory::GetDefaultFactory());
    factories.emplace_back(ManagedBookmarkServiceFactory::GetInstance(),
                           ManagedBookmarkServiceFactory::GetDefaultFactory());
    factories.emplace_back(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating(
            &PriceTrackingIconViewIntegrationTest::BuildMockShoppingService));
    return factories;
  }

  static std::unique_ptr<KeyedService> BuildMockShoppingService(
      content::BrowserContext* context) {
    std::unique_ptr<commerce::MockShoppingService> service =
        std::make_unique<commerce ::MockShoppingService>();
    service->SetIsShoppingListEligible(true);
    return service;
  }

  PriceTrackingIconView* GetChip() {
    auto* location_bar_view = browser_view()->toolbar()->location_bar();
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForView(location_bar_view);
    views::View* matched_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kPriceTrackingChipElementId, context);

    return matched_view
               ? views::AsViewClass<PriceTrackingIconView>(matched_view)
               : nullptr;
  }

  void SimulateServerPriceTrackState(bool is_price_tracked) {
    // Ensure the tab helper has the correct value from the "server" before the
    // meta event is triggered.
    ON_CALL(*GetTabHelper(), IsPriceTracking)
        .WillByDefault(testing::Return(is_price_tracked));

    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

    bookmarks::AddIfNotBookmarked(bookmark_model, GURL(kTrackableUrl),
                                  std::u16string());

    commerce::AddProductBookmark(bookmark_model, u"title", GURL(kTrackableUrl),
                                 0, is_price_tracked);
  }

  void SimulateSubscriptionChangeEvent(bool is_subscribed) {
    if (is_subscribed) {
      GetTabHelper()->GetPriceTrackingControllerForTesting()->OnSubscribe(
          commerce::BuildUserSubscriptionForClusterId(0L), true);
    } else {
      GetTabHelper()->GetPriceTrackingControllerForTesting()->OnUnsubscribe(
          commerce::BuildUserSubscriptionForClusterId(0L), true);
    }
    base::RunLoop().RunUntilIdle();
  }

  void VerifyIconState(PriceTrackingIconView* icon_view,
                       bool is_price_tracked) {
    EXPECT_TRUE(icon_view->GetVisible());
    if (is_price_tracked) {
      EXPECT_EQ(icon_view->GetIconLabelForTesting(),
                l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACKING_PRICE));
      EXPECT_STREQ(icon_view->GetVectorIcon().name,
                   omnibox::kPriceTrackingEnabledRefreshIcon.name);
      EXPECT_EQ(icon_view->GetTextForTooltipAndAccessibleName(),
                l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACKING_PRICE));
    } else {
      EXPECT_EQ(icon_view->GetIconLabelForTesting(),
                l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE));
      EXPECT_STREQ(icon_view->GetVectorIcon().name,
                   omnibox::kPriceTrackingDisabledRefreshIcon.name);
      EXPECT_EQ(icon_view->GetTextForTooltipAndAccessibleName(),
                l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE));
    }
  }

  MockCommerceUiTabHelper* GetTabHelper() { return mock_tab_helper_.get(); }

 protected:
  raw_ptr<MockCommerceUiTabHelper, DanglingUntriaged> mock_tab_helper_;
  base::UserActionTester user_action_tester_;

 private:
  base::test::ScopedFeatureList test_features_;
};

TEST_F(PriceTrackingIconViewIntegrationTest,
       PriceTrackingIconViewVisibleOnNavigation) {
  SimulateServerPriceTrackState(/*is_price_tracked=*/true);

  ON_CALL(*GetTabHelper(), ShouldShowPriceTrackingIconView)
      .WillByDefault(testing::Return(true));

  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.OmniboxChipShown"),
            0);

  NavigateAndCommitActiveTab(GURL(kTrackableUrl));

  auto* icon_view = GetChip();
  VerifyIconState(icon_view, /*is_price_tracked=*/true);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.OmniboxChipShown"),
            1);
}

TEST_F(PriceTrackingIconViewIntegrationTest,
       PriceTrackingIconViewInvisibleOnNavigation) {
  SimulateServerPriceTrackState(/*is_price_tracked=*/true);

  ON_CALL(*GetTabHelper(), ShouldShowPriceTrackingIconView)
      .WillByDefault(testing::Return(true));

  NavigateAndCommitActiveTab(GURL(kTrackableUrl));

  auto* icon_view = GetChip();
  EXPECT_TRUE(icon_view->GetVisible());

  ON_CALL(*GetTabHelper(), ShouldShowPriceTrackingIconView)
      .WillByDefault(testing::Return(false));

  NavigateAndCommitActiveTab(GURL(kNonTrackableUrl));
  EXPECT_FALSE(icon_view->GetVisible());
}

TEST_F(PriceTrackingIconViewIntegrationTest,
       IconUpdatedWhenSubscriptionChanged) {
  SimulateServerPriceTrackState(/*is_price_tracked=*/true);

  ON_CALL(*GetTabHelper(), ShouldShowPriceTrackingIconView)
      .WillByDefault(testing::Return(true));
  ON_CALL(*GetTabHelper(), IsPriceTracking)
      .WillByDefault(testing::Return(true));

  NavigateAndCommitActiveTab(GURL(kTrackableUrl));

  auto* icon_view = GetChip();
  VerifyIconState(icon_view, /*is_price_tracked=*/true);

  // Simulate meta data changed.
  SimulateServerPriceTrackState(false);
  SimulateSubscriptionChangeEvent(false);

  VerifyIconState(icon_view, /*is_price_tracked=*/false);
}
