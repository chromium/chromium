// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/test/widget_activation_waiter.h"

namespace base::test {

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

}  // namespace base::test
