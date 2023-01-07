// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_tracking_icon_view.h"

#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/price_tracking/mock_shopping_list_ui_tab_helper.h"
#include "chrome/browser/ui/views/commerce/price_tracking_bubble_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
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
const char kTestURL[] = "about:blank";
}  // namespace

class PriceTrackingIconViewInteractiveTest : public InProcessBrowserTest {
 public:
  PriceTrackingIconViewInteractiveTest() {
    test_features_.InitAndEnableFeature(commerce::kShoppingList);
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

    bookmarks::AddIfNotBookmarked(bookmark_model, GURL(kTestURL),
                                  std::u16string());

    MockShoppingListUiTabHelper::CreateForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
    mock_tab_helper_ = static_cast<MockShoppingListUiTabHelper*>(
        MockShoppingListUiTabHelper::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents()));
    EXPECT_CALL(*mock_tab_helper_, GetProductImage);
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
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());

    commerce::AddProductBookmark(bookmark_model, u"title", GURL(kTestURL), 0,
                                 is_price_tracked);
  }

 protected:
  base::UserActionTester user_action_tester_;
  raw_ptr<MockShoppingListUiTabHelper> mock_tab_helper_;

 private:
  base::test::ScopedFeatureList test_features_;
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
  commerce::AddProductBookmark(bookmark_model, u"title", GURL(kTestURL), 0,
                               true);

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

class PriceTrackingBubbleInteractiveTest
    : public PriceTrackingIconViewInteractiveTest {
 public:
  PriceTrackingBubbleInteractiveTest() = default;

  PriceTrackingBubbleInteractiveTest(
      const PriceTrackingBubbleInteractiveTest&) = delete;
  PriceTrackingBubbleInteractiveTest& operator=(
      const PriceTrackingBubbleInteractiveTest&) = delete;

  ~PriceTrackingBubbleInteractiveTest() override = default;

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
