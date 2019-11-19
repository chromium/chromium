// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/shell_test_api.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/perf/performance_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/test/ui_controls.h"

// Test overview enter/exit animations with following conditions
// int: number of windows : 2, 8
// bool: the tab content (chrome://blank, chrome://newtab)
// bool: tablet mode, if true.
// TODO(oshima): Add Tablet/SplitView mode.
class OverviewAnimationsTest
    : public UIPerformanceTest,
      public testing::WithParamInterface<::testing::tuple<int, bool, bool>> {
 public:
  OverviewAnimationsTest() = default;
  ~OverviewAnimationsTest() override = default;

  // UIPerformanceTest:
  void SetUpOnMainThread() override {
    UIPerformanceTest::SetUpOnMainThread();

    int additional_browsers = std::get<0>(GetParam()) - 1;
    bool blank_page = std::get<1>(GetParam());
    tablet_mode_ = std::get<2>(GetParam());

    if (tablet_mode_)
      ash::ShellTestApi().SetTabletModeEnabledForTest(true);

    GURL ntp_url("chrome://newtab");
    // The default is blank page.
    if (blank_page)
      ui_test_utils::NavigateToURL(browser(), ntp_url);

    for (int i = additional_browsers; i > 0; i--) {
      Browser* new_browser = CreateBrowser(browser()->profile());
      if (blank_page)
        ui_test_utils::NavigateToURL(new_browser, ntp_url);
    }

    float cost_per_browser = blank_page ? 0.5 : 0.1;
    int wait_seconds = (base::SysInfo::IsRunningOnChromeOS() ? 5 : 0) +
                       additional_browsers * cost_per_browser;
    base::RunLoop run_loop;
    base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                          base::TimeDelta::FromSeconds(wait_seconds));
    run_loop.Run();
  }

  // UIPerformanceTest:
  std::vector<std::string> GetUMAHistogramNames() const override {
    if (tablet_mode_) {
      return {"Ash.Overview.AnimationSmoothness.Enter.TabletMode",
              "Ash.Overview.AnimationSmoothness.Exit.TabletMode"};
    }
    return {"Ash.Overview.AnimationSmoothness.Enter.ClamshellMode",
            "Ash.Overview.AnimationSmoothness.Exit.ClamshellMode"};
  }

 private:
  bool tablet_mode_ = false;
  DISALLOW_COPY_AND_ASSIGN(OverviewAnimationsTest);
};

IN_PROC_BROWSER_TEST_P(OverviewAnimationsTest, EnterExit) {
  // Browser window is used just to identify display.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  gfx::NativeWindow browser_window =
      browser_view->GetWidget()->GetNativeWindow();

  ui_controls::SendKeyPress(browser_window, ui::VKEY_MEDIA_LAUNCH_APP1,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /*command=*/false);
  ash::ShellTestApi().WaitForOverviewAnimationState(
      ash::OverviewAnimationState::kEnterAnimationComplete);
  ui_controls::SendKeyPress(browser_window, ui::VKEY_MEDIA_LAUNCH_APP1,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /*command=*/false);
  ash::ShellTestApi().WaitForOverviewAnimationState(
      ash::OverviewAnimationState::kExitAnimationComplete);
}

INSTANTIATE_TEST_SUITE_P(,
                         OverviewAnimationsTest,
                         ::testing::Combine(::testing::Values(2, 8),
                                            /*blank=*/testing::Bool(),
                                            /*tablet=*/testing::Bool()));
