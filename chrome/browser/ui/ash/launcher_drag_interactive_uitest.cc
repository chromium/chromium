// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/perf/drag_event_generator.h"
#include "chrome/test/base/perf/performance_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

// Test launcher drag performance in clamshell mode.
class LauncherDragClamshellModeTest : public UIPerformanceTest {
 public:
  LauncherDragClamshellModeTest() = default;
  ~LauncherDragClamshellModeTest() override = default;

  // UIPerformanceTest:
  void SetUpOnMainThread() override {
    UIPerformanceTest::SetUpOnMainThread();

    test::PopulateDummyAppListItems(100);
    // Ash may not be ready to receive events right away.
    int warmup_seconds = base::SysInfo::IsRunningOnChromeOS() ? 5 : 1;
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromSeconds(warmup_seconds));
    run_loop.Run();
  }

  // UIPerformanceTest:
  std::vector<std::string> GetUMAHistogramNames() const override {
    return {base::StringPrintf(
        "Apps.StateTransition.Drag.PresentationTime.ClamshellMode")};
  }

  static gfx::Rect GetDisplayBounds(aura::Window* window) {
    return display::Screen::GetScreen()
        ->GetDisplayNearestWindow(window)
        .bounds();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherDragClamshellModeTest);
};

// Drag to open the launcher from shelf. In tablet mode, swiping up from shelf
// no longer drags up launcher, so we only need to test it in clamshell mode.
IN_PROC_BROWSER_TEST_F(LauncherDragClamshellModeTest, Open) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  ash::ShellTestApi shell_test_api;

  gfx::Rect display_bounds = GetDisplayBounds(browser_window);
  gfx::Point start_point = gfx::Point(
      display_bounds.width() / 4,
      display_bounds.bottom() - ash::ShelfConfig::Get()->shelf_size() / 2);
  gfx::Point end_point(start_point);
  end_point.set_y(10);
  auto generator = ui_test_utils::DragEventGenerator::CreateForTouch(
      std::make_unique<ui_test_utils::InterpolatedProducer>(
          start_point, end_point, base::TimeDelta::FromMilliseconds(1000)));
  generator->Wait();

  shell_test_api.WaitForLauncherAnimationState(
      ash::AppListViewState::kFullscreenAllApps);
}

// Test launcher drag performace in both clamshell mode and tablet mode.
class LauncherDragTest : public LauncherDragClamshellModeTest,
                         public ::testing::WithParamInterface<bool> {
 public:
  LauncherDragTest() {
    tablet_mode_ = GetParam();

    // Drag from top to close app list in tablet mode is disabled if
    // shelf hotseat feature is enabled.
    if (tablet_mode_) {
      scoped_features_.InitWithFeatures({},
                                        {chromeos::features::kShelfHotseat});
    }
  }
  ~LauncherDragTest() override = default;

  // UIPerformanceTest:
  void SetUpOnMainThread() override {
    if (tablet_mode_)
      ash::ShellTestApi().SetTabletModeEnabledForTest(true);

    LauncherDragClamshellModeTest::SetUpOnMainThread();
  }

  // UIPerformanceTest:
  std::vector<std::string> GetUMAHistogramNames() const override {
    return {
        base::StringPrintf("Apps.StateTransition.Drag.PresentationTime.%s",
                           tablet_mode_ ? "TabletMode" : "ClamshellMode"),
    };
  }

 private:
  bool tablet_mode_ = false;
  base::test::ScopedFeatureList scoped_features_;

  DISALLOW_COPY_AND_ASSIGN(LauncherDragTest);
};

IN_PROC_BROWSER_TEST_P(LauncherDragTest, Close) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  ash::ShellTestApi shell_test_api;
  ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                            /*control=*/false,
                            /*shift=*/true,
                            /*alt=*/false,
                            /*command=*/false);
  shell_test_api.WaitForLauncherAnimationState(
      ash::AppListViewState::kFullscreenAllApps);

  gfx::Rect display_bounds = GetDisplayBounds(browser_window);
  gfx::Point start_point = gfx::Point(display_bounds.width() / 4, 10);
  gfx::Point end_point(start_point);
  end_point.set_y(display_bounds.bottom() -
                  ash::ShelfConfig::Get()->shelf_size() / 2);
  auto generator = ui_test_utils::DragEventGenerator::CreateForTouch(
      std::make_unique<ui_test_utils::InterpolatedProducer>(
          start_point, end_point, base::TimeDelta::FromMilliseconds(1000)));
  generator->Wait();

  shell_test_api.WaitForLauncherAnimationState(ash::AppListViewState::kClosed);
}

INSTANTIATE_TEST_SUITE_P(All,
                         LauncherDragTest,
                         /*tablet_mode=*/::testing::Bool());
