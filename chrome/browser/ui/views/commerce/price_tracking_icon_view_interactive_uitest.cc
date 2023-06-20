// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_tracking_icon_view.h"

#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/price_tracking/mock_shopping_list_ui_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/commerce/price_tracking_bubble_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/test/feature_promo_test_util.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {
const char kNonTrackableUrl[] = "http://google.com";
const char kTrackableUrl[] = "about:blank";
const char kNonBookmarkedUrl[] = "about:blank?bookmarked=false";
}  // namespace

class PriceTrackingIconViewInteractiveTest : public InProcessBrowserTest {
 public:
  PriceTrackingIconViewInteractiveTest() {
    test_features_.InitAndEnableFeatures(
        {commerce::kShoppingList,
         feature_engagement::kIPHPriceTrackingInSidePanelFeature},
        {});
  }

  PriceTrackingIconViewInteractiveTest(
      const PriceTrackingIconViewInteractiveTest&) = delete;
  PriceTrackingIconViewInteractiveTest& operator=(
      const PriceTrackingIconViewInteractiveTest&) = delete;

  ~PriceTrackingIconViewInteractiveTest() override = default;

  void SetUpOnMainThread() override {
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

    // Remove the original tab helper so we don't get into a bad situation when
    // we go to replace the shopping service with the mock one. The old tab
    // helper is still holding a reference to the original shopping service and
    // other dependencies which we switch out below (leaving some dangling
    // pointers on destruction).
    browser()->tab_strip_model()->GetActiveWebContents()->RemoveUserData(
        commerce::ShoppingListUiTabHelper::UserDataKey());

    mock_shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                browser()->profile(),
                base::BindRepeating([](content::BrowserContext* context) {
                  return commerce::MockShoppingService::Build();
                })));

    MockShoppingListUiTabHelper::CreateForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
    mock_tab_helper_ = static_cast<MockShoppingListUiTabHelper*>(
        MockShoppingListUiTabHelper::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents()));
    mock_tab_helper_->SetShoppingServiceForTesting(mock_shopping_service_);

    const gfx::Image image = mock_tab_helper_->GetValidProductImage();
    ON_CALL(*mock_tab_helper_, GetProductImage)
        .WillByDefault(
            testing::ReturnRef(mock_tab_helper_->GetValidProductImage()));
  }

  PriceTrackingIconView* GetChip() {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    auto* location_bar_view = browser_view->toolbar()->location_bar();
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForView(location_bar_view);
    views::View* matched_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kPriceTrackingChipElementId, context);

    return matched_view
               ? views::AsViewClass<PriceTrackingIconView>(matched_view)
               : nullptr;
  }

  void ClickPriceTrackingIconView() {
    // TODO(meiliang@): Investigte why calling
    // ui_test_utils::ClickOnView(GetChip()) does not work.
    views::test::ButtonTestApi(GetChip()).NotifyClick(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    base::RunLoop().RunUntilIdle();
  }

  void SimulateServerPriceTrackStateUpdated(bool is_price_tracked) {
    // Ensure the tab helper has the correct value from the "server" before the
    // meta event is triggered.
    ON_CALL(*mock_tab_helper_, IsPriceTracking)
        .WillByDefault(testing::Return(is_price_tracked));

    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());

    commerce::AddProductBookmark(bookmark_model, u"title", GURL(kTrackableUrl),
                                 0, is_price_tracked);
  }

  StarView* GetBookmarkStar() {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    auto* location_bar_view = browser_view->toolbar()->location_bar();
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForView(location_bar_view);
    views::View* matched_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kBookmarkStarViewElementId, context);

    return matched_view ? views::AsViewClass<StarView>(matched_view) : nullptr;
  }

  const std::u16string& GetDefaultFolderName() {
    bookmarks::BookmarkModel* const model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    const bookmarks::BookmarkNode* node = model->other_node();
    return node->GetTitle();
  }

  void WaitForIconFinishAnimating(PriceTrackingIconView* icon_view) {
    while (icon_view->is_animating_label()) {
      base::RunLoop().RunUntilIdle();
    }
  }

 protected:
  base::UserActionTester user_action_tester_;
  raw_ptr<commerce::MockShoppingService, DanglingUntriaged>
      mock_shopping_service_;
  raw_ptr<MockShoppingListUiTabHelper, DanglingUntriaged> mock_tab_helper_;

 private:
  feature_engagement::test::ScopedIphFeatureList test_features_;
};

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       FUEBubbleShownOnPress) {
  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);

  ClickPriceTrackingIconView();
  EXPECT_TRUE(icon_view->GetBubble());
  auto* bubble =
      static_cast<PriceTrackingBubbleDialogView*>(icon_view->GetBubble());
  EXPECT_EQ(bubble->GetTypeForTesting(),
            PriceTrackingBubbleDialogView::Type::TYPE_FIRST_USE_EXPERIENCE);
}

IN_PROC_BROWSER_TEST_F(
    PriceTrackingIconViewInteractiveTest,
    PriceTrackingBubbleShownOnPress_BeforeFUEOnTrackedProduct) {
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble));
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  commerce::AddProductBookmark(bookmark_model, u"title", GURL(kTrackableUrl), 0,
                               true);
  ON_CALL(*mock_tab_helper_, IsPriceTracking)
      .WillByDefault(testing::Return(true));

  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/true);

  ClickPriceTrackingIconView();
  EXPECT_TRUE(icon_view->GetBubble());
  auto* bubble =
      static_cast<PriceTrackingBubbleDialogView*>(icon_view->GetBubble());
  EXPECT_EQ(bubble->GetTypeForTesting(),
            PriceTrackingBubbleDialogView::Type::TYPE_NORMAL);
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       PriceTrackingBubbleShownOnPress_AfterFUE) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);
  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);

  ClickPriceTrackingIconView();
  EXPECT_TRUE(icon_view->GetBubble());
  auto* bubble =
      static_cast<PriceTrackingBubbleDialogView*>(icon_view->GetBubble());
  EXPECT_EQ(bubble->GetTypeForTesting(),
            PriceTrackingBubbleDialogView::Type::TYPE_NORMAL);
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       BubbleCanBeReshowOnPress) {
  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);

  ClickPriceTrackingIconView();
  EXPECT_TRUE(icon_view->GetBubble());
  auto* widget = GetChip()->GetBubble()->GetWidget();
  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  widget->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
  destroyed_waiter.Wait();
  EXPECT_FALSE(icon_view->GetBubble());

  // Click the icon again to reshow the bubble.
  EXPECT_CALL(*mock_tab_helper_, GetProductImage);
  ClickPriceTrackingIconView();
  EXPECT_TRUE(icon_view->GetBubble());
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       EnablePriceTrackOnPress) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);
  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);
  EXPECT_EQ(icon_view->GetIconLabelForTesting(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE));
  EXPECT_STREQ(icon_view->GetVectorIcon().name,
               omnibox::kPriceTrackingDisabledIcon.name);
  EXPECT_EQ(icon_view->GetTextForTooltipAndAccessibleName(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE));

  ClickPriceTrackingIconView();
  EXPECT_TRUE(icon_view->GetBubble());
  SimulateServerPriceTrackStateUpdated(/*is_price_tracked=*/true);

  EXPECT_EQ(icon_view->GetIconLabelForTesting(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACKING_PRICE));
  EXPECT_STREQ(icon_view->GetVectorIcon().name,
               omnibox::kPriceTrackingEnabledFilledIcon.name);
  EXPECT_EQ(icon_view->GetTextForTooltipAndAccessibleName(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACKING_PRICE));
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       CreateBookmarkOnPressIfNotExist) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);
  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);

  GURL url = GURL(kTrackableUrl);
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  EXPECT_FALSE(bookmarks::IsBookmarkedByUser(bookmark_model, url));

  ClickPriceTrackingIconView();
  EXPECT_TRUE(bookmarks::IsBookmarkedByUser(bookmark_model, url));

  const bookmarks::BookmarkNode* node =
      bookmark_model->GetMostRecentlyAddedUserNodeForURL(url);
  EXPECT_EQ(node->parent()->GetTitle(), GetDefaultFolderName());
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       RecordOmniboxChipClicked) {
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.OmniboxChipClicked"),
            0);
  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);
  ClickPriceTrackingIconView();
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.OmniboxChipClicked"),
            1);
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       RecordOmniboxChipTracked) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);

  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.OmniboxChip.Tracked"),
            0);
  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);
  ClickPriceTrackingIconView();
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.OmniboxChip.Tracked"),
            1);
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       NoRecordOmniboxChipTracked_ForTrackedProduct) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);

  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.OmniboxChip.Tracked"),
            0);
  auto* icon_view = GetChip();
  SimulateServerPriceTrackStateUpdated(/*is_price_tracked=*/true);
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/true);
  ClickPriceTrackingIconView();
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.OmniboxChip.Tracked"),
            0);
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       NoRecordOmniboxChipTracked_ForFUEFlow) {
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.OmniboxChip.Tracked"),
            0);
  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);
  ClickPriceTrackingIconView();
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.OmniboxChip.Tracked"),
            0);
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       TrackedProductIsDifferentBookmark) {
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());

  const uint64_t cluster_id = 12345L;
  commerce::AddProductBookmark(bookmark_model, u"title",
                               GURL("https://example.com"), cluster_id, true);

  ON_CALL(*mock_tab_helper_, ShouldShowPriceTrackingIconView)
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_tab_helper_, IsPriceTracking)
      .WillByDefault(testing::Return(true));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kNonBookmarkedUrl)));

  EXPECT_STREQ(GetChip()->GetVectorIcon().name,
               omnibox::kPriceTrackingEnabledFilledIcon.name);
  EXPECT_FALSE(GetBookmarkStar()->GetActive());
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       IconViewAccessibleName) {
  EXPECT_EQ(GetChip()->GetAccessibleName(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE));
  EXPECT_EQ(GetChip()->GetTextForTooltipAndAccessibleName(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE));
}

class PriceTrackingIconViewErrorHandelingTest
    : public PriceTrackingIconViewInteractiveTest {
 public:
  PriceTrackingIconViewErrorHandelingTest() {
    test_features_.InitWithFeaturesAndParameters(
        {{commerce::kShoppingList,
          {{commerce::kRevertIconOnFailureParam, "true"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList test_features_;
};

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewErrorHandelingTest,
                       IconRevertedOnFailure) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);
  SimulateServerPriceTrackStateUpdated(/*is_price_tracked=*/false);
  ON_CALL(*mock_tab_helper_, ShouldShowPriceTrackingIconView)
      .WillByDefault(testing::Return(true));

  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);
  EXPECT_EQ(icon_view->GetIconLabelForTesting(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE));
  EXPECT_STREQ(icon_view->GetVectorIcon().name,
               omnibox::kPriceTrackingDisabledIcon.name);
  EXPECT_EQ(icon_view->GetTextForTooltipAndAccessibleName(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE));

  // Simulate the failure.
  mock_shopping_service_->SetSubscribeCallbackValue(false);

  ON_CALL(*mock_tab_helper_, SetPriceTrackingState)
      .WillByDefault([](bool enable, bool is_new_bookmark,
                        base::OnceCallback<void(bool)> callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), false));
      });

  ClickPriceTrackingIconView();

  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_EQ(icon_view->GetIconLabelForTesting(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE));
  EXPECT_STREQ(icon_view->GetVectorIcon().name,
               omnibox::kPriceTrackingDisabledIcon.name);
  EXPECT_EQ(icon_view->GetTextForTooltipAndAccessibleName(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE));
  EXPECT_FALSE(icon_view->GetBubble());
}

class PriceTrackingIconViewEngagementTest
    : public PriceTrackingIconViewInteractiveTest {
 public:
  PriceTrackingIconViewEngagementTest() {
    test_features_.InitAndEnableFeatures(
        {commerce::kShoppingList,
         feature_engagement::kIPHPriceTrackingPageActionIconLabelFeature});
  }

  void SetUpOnMainThread() override {
    PriceTrackingIconViewInteractiveTest::SetUpOnMainThread();

    BrowserFeaturePromoController* const promo_controller =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->GetFeaturePromoController();
    EXPECT_TRUE(
        user_education::test::WaitForFeatureEngagementReady(promo_controller));

    SetUpChip();
  }

  void SetUpChip() {
    SimulateServerPriceTrackStateUpdated(/*is_price_tracked=*/false);
    ON_CALL(*mock_tab_helper_, ShouldShowPriceTrackingIconView)
        .WillByDefault(testing::Return(true));
  }

 private:
  feature_engagement::test::ScopedIphFeatureList test_features_;
};

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewEngagementTest, ShowExpandedIcon) {
  auto* icon_view = GetChip();
  EXPECT_FALSE(icon_view->GetVisible());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTrackableUrl)));
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_TRUE(icon_view->ShouldShowLabel());
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewEngagementTest,
                       ExpandedIconShownOnceOnly) {
  auto* icon_view = GetChip();
  EXPECT_FALSE(icon_view->GetVisible());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTrackableUrl)));
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_TRUE(icon_view->ShouldShowLabel());

  ON_CALL(*mock_tab_helper_, ShouldShowPriceTrackingIconView)
      .WillByDefault(testing::Return(false));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kNonTrackableUrl)));
  EXPECT_FALSE(icon_view->GetVisible());
  EXPECT_FALSE(icon_view->ShouldShowLabel());

  ON_CALL(*mock_tab_helper_, ShouldShowPriceTrackingIconView)
      .WillByDefault(testing::Return(true));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTrackableUrl)));
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_FALSE(icon_view->ShouldShowLabel());
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewEngagementTest, AutoCollapseIcon) {
  auto* icon_view = GetChip();
  EXPECT_FALSE(icon_view->GetVisible());
  base::MockOneShotTimer timer;
  icon_view->SetOneShotTimerForTesting(&timer);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTrackableUrl)));
  WaitForIconFinishAnimating(icon_view);
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_TRUE(icon_view->ShouldShowLabel());

  // Simulate ready to collapse the icon.
  timer.Fire();

  WaitForIconFinishAnimating(icon_view);
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_FALSE(icon_view->ShouldShowLabel());
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewEngagementTest,
                       StopCollapseTimerWhenClickingIcon) {
  auto* icon_view = GetChip();
  EXPECT_FALSE(icon_view->GetVisible());
  base::MockOneShotTimer timer;
  icon_view->SetOneShotTimerForTesting(&timer);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTrackableUrl)));
  WaitForIconFinishAnimating(icon_view);
  EXPECT_TRUE(icon_view->GetVisible());

  ClickPriceTrackingIconView();
  EXPECT_FALSE(timer.IsRunning());
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_TRUE(icon_view->ShouldShowLabel());
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewEngagementTest,
                       CollapseIconUponBubbleClosing) {
  auto* icon_view = GetChip();
  EXPECT_FALSE(icon_view->GetVisible());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTrackableUrl)));
  WaitForIconFinishAnimating(icon_view);
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_TRUE(icon_view->ShouldShowLabel());

  ClickPriceTrackingIconView();
  views::test::WidgetVisibleWaiter(icon_view->GetBubble()->GetWidget()).Wait();
  EXPECT_TRUE(icon_view->GetBubble());

  // Close bubble and verify the label is hiding.
  auto* widget = icon_view->GetBubble()->GetWidget();
  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  widget->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
  destroyed_waiter.Wait();
  EXPECT_FALSE(icon_view->GetBubble());
  WaitForIconFinishAnimating(icon_view);
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_FALSE(icon_view->ShouldShowLabel());
}

class PriceTrackingBubbleInteractiveTest
    : public PriceTrackingIconViewInteractiveTest {
 public:
  PriceTrackingBubbleInteractiveTest() = default;

  PriceTrackingBubbleInteractiveTest(
      const PriceTrackingBubbleInteractiveTest&) = delete;
  PriceTrackingBubbleInteractiveTest& operator=(
      const PriceTrackingBubbleInteractiveTest&) = delete;

  ~PriceTrackingBubbleInteractiveTest() override = default;
};

IN_PROC_BROWSER_TEST_F(PriceTrackingBubbleInteractiveTest,
                       TrackPriceOnFUEBubble) {
  // Show PriceTackingIconView.
  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);

  // Verify the PriceTackingIconView original state.
  EXPECT_EQ(icon_view->GetIconLabelForTesting(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE));
  EXPECT_STREQ(icon_view->GetVectorIcon().name,
               omnibox::kPriceTrackingDisabledIcon.name);
  EXPECT_EQ(icon_view->GetTextForTooltipAndAccessibleName(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE));

  // Click PriceTackingIconView and show the PriceTrackingBubble.
  ClickPriceTrackingIconView();
  auto* bubble =
      static_cast<PriceTrackingBubbleDialogView*>(icon_view->GetBubble());
  EXPECT_TRUE(bubble);
  EXPECT_EQ(bubble->GetTypeForTesting(),
            PriceTrackingBubbleDialogView::Type::TYPE_FIRST_USE_EXPERIENCE);

  // Click the Accept(Track price) bubble.
  bubble->Accept();
  SimulateServerPriceTrackStateUpdated(/*is_price_tracked=*/true);

  // Verify the PriceTackingIconView updates its state.
  EXPECT_EQ(icon_view->GetIconLabelForTesting(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACKING_PRICE));
  EXPECT_STREQ(icon_view->GetVectorIcon().name,
               omnibox::kPriceTrackingEnabledFilledIcon.name);
  EXPECT_EQ(icon_view->GetTextForTooltipAndAccessibleName(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACKING_PRICE));
  EXPECT_TRUE(GetBookmarkStar()->GetActive());
}

IN_PROC_BROWSER_TEST_F(PriceTrackingBubbleInteractiveTest,
                       NotTriggerSidePanelIPH) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kShouldShowPriceTrackFUEBubble, false);
  auto* promo_controller = BrowserView::GetBrowserViewForBrowser(browser())
                               ->GetFeaturePromoController();
  EXPECT_TRUE(
      user_education::test::WaitForFeatureEngagementReady(promo_controller));

  // Trigger IPH now so it won't be triggred later.
  EXPECT_TRUE(
      promo_controller->feature_engagement_tracker()->ShouldTriggerHelpUI(
          feature_engagement::kIPHPriceTrackingInSidePanelFeature));
  EXPECT_FALSE(
      promo_controller->feature_engagement_tracker()->WouldTriggerHelpUI(
          feature_engagement::kIPHPriceTrackingInSidePanelFeature));

  // Show PriceTackingIconView.
  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);

  // Click PriceTackingIconView and show the PriceTrackingBubble.
  ClickPriceTrackingIconView();
  auto* bubble =
      static_cast<PriceTrackingBubbleDialogView*>(icon_view->GetBubble());
  EXPECT_TRUE(bubble);
  EXPECT_EQ(bubble->GetTypeForTesting(),
            PriceTrackingBubbleDialogView::Type::TYPE_NORMAL);

  // Click the Accept(Track price) bubble.
  bubble->Accept();
  SimulateServerPriceTrackStateUpdated(/*is_price_tracked=*/true);

  // Verify IPH is not showing.
  EXPECT_FALSE(promo_controller->IsPromoActive(
      feature_engagement::kIPHPriceTrackingInSidePanelFeature));
}

IN_PROC_BROWSER_TEST_F(PriceTrackingBubbleInteractiveTest,
                       UnrackPriceOnNormalBubble) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);

  // Show PriceTackingIconView.
  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);

  // Click PriceTackingIconView and show the PriceTrackingBubble.
  ClickPriceTrackingIconView();
  auto* bubble =
      static_cast<PriceTrackingBubbleDialogView*>(icon_view->GetBubble());
  EXPECT_TRUE(bubble);
  EXPECT_EQ(bubble->GetTypeForTesting(),
            PriceTrackingBubbleDialogView::Type::TYPE_NORMAL);
  SimulateServerPriceTrackStateUpdated(/*is_price_tracked=*/true);

  // Verify the PriceTackingIconView state before cancel.
  EXPECT_EQ(icon_view->GetIconLabelForTesting(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACKING_PRICE));
  EXPECT_STREQ(icon_view->GetVectorIcon().name,
               omnibox::kPriceTrackingEnabledFilledIcon.name);
  EXPECT_EQ(icon_view->GetTextForTooltipAndAccessibleName(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACKING_PRICE));

  // Click the Cancel(Untrack) button.
  bubble->Cancel();
  SimulateServerPriceTrackStateUpdated(/*is_price_tracked=*/false);

  // Verify the PriceTackingIconView updates its state.
  EXPECT_EQ(icon_view->GetIconLabelForTesting(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE));
  EXPECT_STREQ(icon_view->GetVectorIcon().name,
               omnibox::kPriceTrackingDisabledIcon.name);
  EXPECT_EQ(icon_view->GetTextForTooltipAndAccessibleName(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE));
}

IN_PROC_BROWSER_TEST_F(PriceTrackingBubbleInteractiveTest,
                       RecordFirstRunBubbleShown) {
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.FirstRunBubbleShown"),
            0);

  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);

  ClickPriceTrackingIconView();
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.FirstRunBubbleShown"),
            1);
}

IN_PROC_BROWSER_TEST_F(PriceTrackingBubbleInteractiveTest,
                       RecordFirstRunBubblTrackedPrice) {
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.FirstRunBubbleTrackedPrice"),
            0);

  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);

  ClickPriceTrackingIconView();
  auto* bubble =
      static_cast<PriceTrackingBubbleDialogView*>(icon_view->GetBubble());
  EXPECT_TRUE(bubble);
  bubble->Accept();

  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.FirstRunBubbleTrackedPrice"),
            1);
}

IN_PROC_BROWSER_TEST_F(PriceTrackingBubbleInteractiveTest,
                       RecordFirstRunBubbleDismissed) {
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.FirstRunBubbleDismissed"),
            0);

  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);

  ClickPriceTrackingIconView();
  auto* bubble =
      static_cast<PriceTrackingBubbleDialogView*>(icon_view->GetBubble());
  EXPECT_TRUE(bubble);
  bubble->Cancel();

  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.FirstRunBubbleDismissed"),
            1);
}

IN_PROC_BROWSER_TEST_F(PriceTrackingBubbleInteractiveTest,
                       RecordConfirmationShown) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.ConfirmationShown"),
            0);

  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);

  ClickPriceTrackingIconView();
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.ConfirmationShown"),
            1);
}

IN_PROC_BROWSER_TEST_F(PriceTrackingBubbleInteractiveTest,
                       RecordConfirmationUntracked) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.Confirmation.Untrack"),
            0);

  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);

  ClickPriceTrackingIconView();
  auto* bubble =
      static_cast<PriceTrackingBubbleDialogView*>(icon_view->GetBubble());
  EXPECT_TRUE(bubble);
  bubble->Cancel();

  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.Confirmation.Untrack"),
            1);
}

IN_PROC_BROWSER_TEST_F(PriceTrackingBubbleInteractiveTest,
                       RecordEditedBookmarkFolderFromOmniboxBubble) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShouldShowPriceTrackFUEBubble, false);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.EditedBookmarkFolderFromOmniboxBubble"),
            0);

  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);

  ClickPriceTrackingIconView();
  auto* bubble =
      static_cast<PriceTrackingBubbleDialogView*>(icon_view->GetBubble());
  EXPECT_TRUE(bubble);
  bubble->GetBodyLabelForTesting()->ClickFirstLinkForTesting();

  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Commerce.PriceTracking.EditedBookmarkFolderFromOmniboxBubble"),
            1);
}

class PriceTrackingIconViewUnifiedSidePanelInteractiveTest
    : public PriceTrackingBubbleInteractiveTest {
 public:
  PriceTrackingIconViewUnifiedSidePanelInteractiveTest() {
    test_features_.InitAndEnableFeatures(
        {commerce::kShoppingList,
         feature_engagement::kIPHPriceTrackingInSidePanelFeature});
  }

 private:
  feature_engagement::test::ScopedIphFeatureList test_features_;
};

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewUnifiedSidePanelInteractiveTest,
                       TriggerSidePanelIPH) {
  SidePanelCoordinator* coordinator =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  DCHECK(coordinator);
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kShouldShowPriceTrackFUEBubble, false);
  auto* promo_controller = BrowserView::GetBrowserViewForBrowser(browser())
                               ->GetFeaturePromoController();
  EXPECT_TRUE(
      user_education::test::WaitForFeatureEngagementReady(promo_controller));

  // Show PriceTackingIconView.
  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);

  // Click PriceTackingIconView and show the PriceTrackingBubble.
  ClickPriceTrackingIconView();
  auto* bubble =
      static_cast<PriceTrackingBubbleDialogView*>(icon_view->GetBubble());
  EXPECT_TRUE(bubble);
  EXPECT_EQ(bubble->GetTypeForTesting(),
            PriceTrackingBubbleDialogView::Type::TYPE_NORMAL);

  // Click the Accept(Track price) bubble.
  bubble->Accept();
  SimulateServerPriceTrackStateUpdated(/*is_price_tracked=*/true);

  // Verify IPH is showing and side panel registry is properly set up to force
  // show bookmark tab in side panel.
  EXPECT_TRUE(promo_controller->IsPromoActive(
      feature_engagement::kIPHPriceTrackingInSidePanelFeature));
  SidePanelRegistry* registry =
      SidePanelCoordinator::GetGlobalSidePanelRegistry(browser());
  EXPECT_TRUE(registry->active_entry().has_value());
  EXPECT_EQ(registry->active_entry().value()->key().id(),
            SidePanelEntry::Id::kBookmarks);
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewUnifiedSidePanelInteractiveTest,
                       NotTriggerSidePanelIPH) {
  SidePanelCoordinator* coordinator =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  DCHECK(coordinator);
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kShouldShowPriceTrackFUEBubble, false);
  auto* promo_controller = BrowserView::GetBrowserViewForBrowser(browser())
                               ->GetFeaturePromoController();
  EXPECT_TRUE(
      user_education::test::WaitForFeatureEngagementReady(promo_controller));

  // Trigger IPH now so it won't be triggred later.
  EXPECT_TRUE(
      promo_controller->feature_engagement_tracker()->ShouldTriggerHelpUI(
          feature_engagement::kIPHPriceTrackingInSidePanelFeature));
  EXPECT_FALSE(
      promo_controller->feature_engagement_tracker()->WouldTriggerHelpUI(
          feature_engagement::kIPHPriceTrackingInSidePanelFeature));

  // Show PriceTackingIconView.
  auto* icon_view = GetChip();
  icon_view->ForceVisibleForTesting(/*is_tracking_price=*/false);

  // Click PriceTackingIconView and show the PriceTrackingBubble.
  ClickPriceTrackingIconView();
  auto* bubble =
      static_cast<PriceTrackingBubbleDialogView*>(icon_view->GetBubble());
  EXPECT_TRUE(bubble);
  EXPECT_EQ(bubble->GetTypeForTesting(),
            PriceTrackingBubbleDialogView::Type::TYPE_NORMAL);

  // Click the Accept(Track price) bubble.
  bubble->Accept();
  SimulateServerPriceTrackStateUpdated(/*is_price_tracked=*/true);

  // Verify IPH is not showing and side panel registry is not set up to force
  // show bookmark tab in side panel.
  EXPECT_FALSE(promo_controller->IsPromoActive(
      feature_engagement::kIPHPriceTrackingInSidePanelFeature));
  SidePanelRegistry* registry =
      SidePanelCoordinator::GetGlobalSidePanelRegistry(browser());
  EXPECT_FALSE(registry->active_entry().has_value());
}

class PriceTrackingIconViewAlwaysExpandedTest
    : public PriceTrackingIconViewInteractiveTest {
 public:
  PriceTrackingIconViewAlwaysExpandedTest() {
    test_features_.InitAndEnableFeaturesWithParameters(
        {{feature_engagement::kIPHPriceTrackingPageActionIconLabelFeature,
          GetFeatureEngagementParams()}});
  }

  std::map<std::string, std::string> GetFeatureEngagementParams() {
    return {
        {"availability", "any"},
        {"event_used", "name:used;comparator:any;window:0;storage:360"},
        {"event_trigger", "name:trigger;comparator:any;window:0;storage:360"},
        {"session_rate", "any"}};
  }

 private:
  feature_engagement::test::ScopedIphFeatureList test_features_;
};

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewAlwaysExpandedTest,
                       IconAlwaysIsExpanded) {
  BrowserFeaturePromoController* const promo_controller =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->GetFeaturePromoController();
  EXPECT_TRUE(
      user_education::test::WaitForFeatureEngagementReady(promo_controller));

  SimulateServerPriceTrackStateUpdated(/*is_price_tracked=*/false);
  ON_CALL(*mock_tab_helper_, ShouldShowPriceTrackingIconView)
      .WillByDefault(testing::Return(true));
  auto* icon_view = GetChip();
  EXPECT_FALSE(icon_view->GetVisible());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTrackableUrl)));
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_TRUE(icon_view->ShouldShowLabel());

  // Navigate to a non trackable page.
  ON_CALL(*mock_tab_helper_, ShouldShowPriceTrackingIconView)
      .WillByDefault(testing::Return(false));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kNonTrackableUrl)));
  EXPECT_FALSE(icon_view->GetVisible());
  EXPECT_FALSE(icon_view->ShouldShowLabel());

  // Navigate to a trackable page and verify the icon is expanded again.
  ON_CALL(*mock_tab_helper_, ShouldShowPriceTrackingIconView)
      .WillByDefault(testing::Return(true));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTrackableUrl)));
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_TRUE(icon_view->ShouldShowLabel());
}

class PriceTrackingIconViewIPHTest
    : public PriceTrackingIconViewInteractiveTest {
 public:
  PriceTrackingIconViewIPHTest() {
    int variation_num = static_cast<int>(
        commerce::PriceTrackingChipExperimentVariation::kWithChipIPH);
    test_features_.InitAndEnableFeaturesWithParameters(
        {{feature_engagement::kIPHPriceTrackingPageActionIconLabelFeature, {}},
         {feature_engagement::kIPHPriceTrackingChipFeature, {}},
         {commerce::kCommercePriceTrackingChipExperiment,
          {{commerce::kCommercePriceTrackingChipExperimentVariationParam,
            base::NumberToString(variation_num)}}}});
  }

 private:
  feature_engagement::test::ScopedIphFeatureList test_features_;
};

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewIPHTest, TriggerChipIPH) {
  BrowserFeaturePromoController* const promo_controller =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->GetFeaturePromoController();
  EXPECT_TRUE(
      user_education::test::WaitForFeatureEngagementReady(promo_controller));

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kShouldShowPriceTrackFUEBubble, true);

  SimulateServerPriceTrackStateUpdated(/*is_price_tracked=*/false);
  ON_CALL(*mock_tab_helper_, ShouldShowPriceTrackingIconView)
      .WillByDefault(testing::Return(true));
  auto* icon_view = GetChip();
  EXPECT_FALSE(icon_view->GetVisible());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTrackableUrl)));
  WaitForIconFinishAnimating(icon_view);
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_TRUE(icon_view->ShouldShowLabel());
  EXPECT_TRUE(promo_controller->IsPromoActive(
      feature_engagement::kIPHPriceTrackingChipFeature));
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewIPHTest,
                       NotTriggerChipIPH_AfterTriggered) {
  BrowserFeaturePromoController* const promo_controller =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->GetFeaturePromoController();
  EXPECT_TRUE(
      user_education::test::WaitForFeatureEngagementReady(promo_controller));

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kShouldShowPriceTrackFUEBubble, true);

  // Trigger IPH now so it won't be triggred later.
  EXPECT_TRUE(
      promo_controller->feature_engagement_tracker()->ShouldTriggerHelpUI(
          feature_engagement::kIPHPriceTrackingChipFeature));
  promo_controller->feature_engagement_tracker()->Dismissed(
      feature_engagement::kIPHPriceTrackingChipFeature);

  SimulateServerPriceTrackStateUpdated(/*is_price_tracked=*/false);
  ON_CALL(*mock_tab_helper_, ShouldShowPriceTrackingIconView)
      .WillByDefault(testing::Return(true));
  auto* icon_view = GetChip();
  EXPECT_FALSE(icon_view->GetVisible());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTrackableUrl)));
  WaitForIconFinishAnimating(icon_view);
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_TRUE(icon_view->ShouldShowLabel());
  EXPECT_FALSE(promo_controller->IsPromoActive(
      feature_engagement::kIPHPriceTrackingChipFeature));
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewIPHTest,
                       NotTriggerChipIPH_ForNormalBubble) {
  BrowserFeaturePromoController* const promo_controller =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->GetFeaturePromoController();
  EXPECT_TRUE(
      user_education::test::WaitForFeatureEngagementReady(promo_controller));

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kShouldShowPriceTrackFUEBubble, false);

  // Trigger IPH now so it won't be triggred later.
  EXPECT_TRUE(
      promo_controller->feature_engagement_tracker()->ShouldTriggerHelpUI(
          feature_engagement::kIPHPriceTrackingChipFeature));
  promo_controller->feature_engagement_tracker()->Dismissed(
      feature_engagement::kIPHPriceTrackingChipFeature);

  SimulateServerPriceTrackStateUpdated(/*is_price_tracked=*/false);
  ON_CALL(*mock_tab_helper_, ShouldShowPriceTrackingIconView)
      .WillByDefault(testing::Return(true));
  auto* icon_view = GetChip();
  EXPECT_FALSE(icon_view->GetVisible());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTrackableUrl)));
  WaitForIconFinishAnimating(icon_view);
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_TRUE(icon_view->ShouldShowLabel());
  EXPECT_FALSE(promo_controller->IsPromoActive(
      feature_engagement::kIPHPriceTrackingChipFeature));
}
