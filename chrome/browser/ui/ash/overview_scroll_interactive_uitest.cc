// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/perf/drag_event_generator.h"
#include "chrome/test/base/perf/performance_test.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

// Test overview scroll performance when the new overview layout is active.
class OverviewScrollTest : public UIPerformanceTest {
 public:
  OverviewScrollTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kNewOverviewLayout);
  }

  ~OverviewScrollTest() override = default;

  // UIPerformanceTest:
  void SetUpOnMainThread() override {
    UIPerformanceTest::SetUpOnMainThread();

    // Create twelve windows total, scrolling is only needed when six or more
    // windows are shown.
    const int additional_browsers = 11;
    for (int i = 0; i < additional_browsers; ++i)
      CreateBrowser(browser()->profile());

    // Ash may not be ready to receive events right away.
    int warmup_ms = (base::SysInfo::IsRunningOnChromeOS() ? 5000 : 1000) +
                    additional_browsers * 100;
    base::RunLoop run_loop;
    base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                          base::TimeDelta::FromMilliseconds(warmup_ms));
    run_loop.Run();
  }

  std::vector<std::string> GetUMAHistogramNames() const override {
    return {
        "Ash.Overview.Scroll.PresentationTime.TabletMode",
    };
  }

  static gfx::Rect GetDisplayBounds(aura::Window* window) {
    return display::Screen::GetScreen()
        ->GetDisplayNearestWindow(window)
        .bounds();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(OverviewScrollTest);
};

IN_PROC_BROWSER_TEST_F(OverviewScrollTest, Basic) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();

  ash::ShellTestApi shell_test_api;
  shell_test_api.SetTabletModeEnabledForTest(/*enabled=*/true);

  ui_controls::SendKeyPress(browser_window, ui::VKEY_MEDIA_LAUNCH_APP1,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /*command=*/false);
  shell_test_api.WaitForOverviewAnimationState(
      ash::OverviewAnimationState::kEnterAnimationComplete);

  // Scroll for the top right corner to the top left corner.
  const gfx::Rect display_bounds = GetDisplayBounds(browser_window);
  const gfx::Point start_point =
      display_bounds.top_right() + gfx::Vector2d(-1, 1);
  const gfx::Point end_point = display_bounds.origin() + gfx::Vector2d(1, 1);
  auto generator = ui_test_utils::DragEventGenerator::CreateForTouch(
      std::make_unique<ui_test_utils::InterpolatedProducer>(
          start_point, end_point, base::TimeDelta::FromMilliseconds(1000)));
  generator->Wait();
}
