// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_tracking_icon_view.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/commerce/price_tracking_bubble_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

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
            PriceTrackingBubbleDialogView::Type::TYPE_FUE);
}

IN_PROC_BROWSER_TEST_F(PriceTrackingIconViewInteractiveTest,
                       PriceTrackingBubbleShownOnPress) {
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
  ClickPriceTrackingIconView();
  EXPECT_TRUE(icon_view->GetBubble());
}
