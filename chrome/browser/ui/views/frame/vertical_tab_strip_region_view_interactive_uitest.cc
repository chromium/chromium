// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/test/widget_activation_waiter.h"

class VerticalTabStripRegionViewInteractiveUiTest
    : public VerticalTabsInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  VerticalTabStripRegionViewInteractiveUiTest() = default;

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
};

// Deactivate() function is flaky on Mac. Test is flaky on Linux Wayland because
// activation events are asynchronous.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_VerifyVerticalTabStripBackground \
  DISABLED_VerifyVerticalTabStripBackground
#else
#define MAYBE_VerifyVerticalTabStripBackground VerifyVerticalTabStripBackground
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewInteractiveUiTest,
                       MAYBE_VerifyVerticalTabStripBackground) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* region_view =
      browser_view->vertical_tab_strip_region_view_for_testing();

  browser_view->Activate();
  EXPECT_TRUE(browser_view->GetWidget()->ShouldPaintAsActive());

  const views::Background* background_while_active =
      region_view->GetBackground();

  browser_view->Deactivate();
  EXPECT_FALSE(browser_view->GetWidget()->ShouldPaintAsActive());

  const views::Background* background_while_inactive =
      region_view->GetBackground();

  // Background should not be recreated, pointers should be the same.
  EXPECT_EQ(background_while_active, background_while_inactive);
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewInteractiveUiTest,
                       LogTimeToSwitchMetric) {
  RunTestSequence(
      PressButton(kToolbarAppMenuButtonElementId),
      WithView(kTabStripElementId,
               [](VerticalTabStripView* view) {
                 // Simulate the OnMouseEntered event which doesn't
                 // happen consistently in Kombucha.
                 ui::MouseEvent event(ui::EventType::kMouseEntered,
                                      gfx::Point(), gfx::Point(),
                                      base::TimeTicks(), 0, 0);
                 view->OnMouseEntered(event);
               }),
      SelectTab(kTabStripElementId, 0),
      CheckResult(
          [this]() {
            return histogram_tester()
                .GetTotalCountsForPrefix("TabStrip.Vertical.TimeToSwitch")
                .size();
          },
          1));
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewInteractiveUiTest,
                       OmniboxPopupSuppressesExpandOnHover) {
  auto* const controller =
      tabs::VerticalTabStripStateController::From(browser());

  controller->RequestCollapse(true);
  controller->SetExpandOnHoverEnabled(true);

  RunScheduledLayouts();

  ui::Accelerator focus_location_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_FOCUS_LOCATION, &focus_location_accelerator));

  RunTestSequence(
      WaitForShow(kVerticalTabStripTopContainerElementId),
      EnsurePresent(kVerticalTabStripTopContainerElementId),

      MoveMouseTo(kVerticalTabStripTopContainerElementId),

      SendAccelerator(kBrowserViewElementId, focus_location_accelerator),

      // Verify that the tab strip remains unexpanded despite the mouse hover.
      CheckViewProperty(kTabStripRegionElementId,
                        &VerticalTabStripRegionView::is_expanded_on_hover,
                        false));
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewInteractiveUiTest,
                       LogAnimationPerMetrics) {
  bool has_fps = false;
  RunTestSequence(
      PressButton(kVerticalTabStripCollapseButtonElementId),
      WaitForEvent(kTabStripRegionElementId,
                   VerticalTabStripRegionView::kAnimationCompletedEvent),
      Do([this, &has_fps]() {
        histogram_tester().ExpectTotalCount(
            "TabStrip.Vertical.Collapse.TimeOfLongestAnimationStep", 1);
        has_fps = histogram_tester().GetTotalCountForPrefix(
                      "TabStrip.Vertical.Collapse.AnimationFPS") > 0;
      }),
      CheckResult(
          [this]() {
            return histogram_tester().GetTotalSum(
                "TabStrip.Vertical.Collapse.TimeOfLongestAnimationStep");
          },
          testing::Gt(0), "Check longest step is nonzero."),
      If([&] { return has_fps; },
         Then(CheckResult(
             [this]() {
               return histogram_tester().GetTotalSum(
                   "TabStrip.Vertical.Collapse.AnimationFPS");
             },
             testing::Gt(0), "Check fps is nonzero.")),
         Else(Log("Compositor failed to render during test."))));
}
