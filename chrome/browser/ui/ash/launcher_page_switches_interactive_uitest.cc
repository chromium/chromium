// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/perf/drag_event_generator.h"
#include "chrome/test/base/perf/performance_test.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {

class PageSwitchWaiter : public ash::PaginationModelObserver {
 public:
  explicit PageSwitchWaiter(ash::PaginationModel* model) : model_(model) {
    model_->AddObserver(this);
  }
  ~PageSwitchWaiter() override { model_->RemoveObserver(this); }

  void Wait() { run_loop_.Run(); }

 private:
  // ash::PaginationModelObserver:
  void TransitionEnded() override { run_loop_.Quit(); }

  ash::PaginationModel* model_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(PageSwitchWaiter);
};

}  // namespace

class LauncherPageSwitchesTest : public UIPerformanceTest,
                                 public ::testing::WithParamInterface<bool> {
 public:
  LauncherPageSwitchesTest() = default;
  ~LauncherPageSwitchesTest() override = default;

  // UIPerformanceTest:
  void SetUpOnMainThread() override {
    UIPerformanceTest::SetUpOnMainThread();
    is_tablet_mode_ = GetParam();

    test::PopulateDummyAppListItems(100);
    if (base::SysInfo::IsRunningOnChromeOS()) {
      base::RunLoop run_loop;
      base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                            base::TimeDelta::FromSeconds(5));
      run_loop.Run();
    }

    ash::ShellTestApi shell_test_api;

    // switch to tablet-mode if necessary.
    if (is_tablet_mode_)
      shell_test_api.SetTabletModeEnabledForTest(true);

    // Open the fullscreen app; required for page switching.
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
    ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                              /*control=*/false,
                              /*shift=*/true,
                              /*alt=*/false,
                              /* command = */ false);
    shell_test_api.WaitForLauncherAnimationState(
        ash::AppListViewState::kFullscreenAllApps);
  }

  // UIPerformanceTest:
  std::vector<std::string> GetUMAHistogramNames() const override {
    return {
        is_tablet_mode_
            ? "Apps.PaginationTransition.AnimationSmoothness.TabletMode"
            : "Apps.PaginationTransition.AnimationSmoothness.ClamshellMode",
    };
  }

 private:
  bool is_tablet_mode_ = false;

  DISALLOW_COPY_AND_ASSIGN(LauncherPageSwitchesTest);
};

IN_PROC_BROWSER_TEST_P(LauncherPageSwitchesTest, SwitchToNextPage) {
  ash::PaginationModel* model = ash::ShellTestApi().GetAppListPaginationModel();
  ASSERT_TRUE(model);
  EXPECT_LT(1, model->total_pages());
  EXPECT_EQ(0, model->selected_page());

  PageSwitchWaiter waiter(model);
  model->SelectPageRelative(1, /*animate=*/true);
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_P(LauncherPageSwitchesTest, SwitchToFarPage) {
  ash::PaginationModel* model = ash::ShellTestApi().GetAppListPaginationModel();
  ASSERT_TRUE(model);
  EXPECT_LT(2, model->total_pages());
  EXPECT_EQ(0, model->selected_page());

  PageSwitchWaiter waiter(model);
  model->SelectPageRelative(2, /*animate=*/true);
  waiter.Wait();
}

INSTANTIATE_TEST_SUITE_P(,
                         LauncherPageSwitchesTest,
                         /*tablet_mode=*/::testing::Bool());

// The test gets very flaky in tablet-mode, so it's in clamshell mode only for
// now.
// TODO(mukai): investigate why and enable this test case with tablet-mode too.
class LauncherPageDragTest : public UIPerformanceTest {
 public:
  LauncherPageDragTest() = default;
  ~LauncherPageDragTest() override = default;

  // UIPerformanceTest:
  void SetUpOnMainThread() override {
    UIPerformanceTest::SetUpOnMainThread();

    test::PopulateDummyAppListItems(100);

    ash::ShellTestApi shell_test_api;

    // Open the fullscreen app; required for page switching.
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
    ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                              /*control=*/false,
                              /*shift=*/true,
                              /*alt=*/false,
                              /* command = */ false);
    shell_test_api.WaitForLauncherAnimationState(
        ash::AppListViewState::kFullscreenAllApps);

    if (base::SysInfo::IsRunningOnChromeOS()) {
      base::RunLoop run_loop;
      base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                            base::TimeDelta::FromSeconds(5));
      run_loop.Run();
    }
  }

  // UIPerformanceTest:
  std::vector<std::string> GetUMAHistogramNames() const override {
    return {
        "Apps.PaginationTransition.DragScroll.PresentationTime.ClamshellMode",
        "Apps.PaginationTransition.DragScroll.PresentationTime.MaxLatency."
        "ClamshellMode",
    };
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherPageDragTest);
};

IN_PROC_BROWSER_TEST_F(LauncherPageDragTest, Run) {
  ash::ShellTestApi shell_test_api;

  gfx::Rect display_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(browser()->window()->GetNativeWindow())
          .bounds();
  gfx::Point start_point = display_bounds.CenterPoint();
  gfx::Point end_point(start_point);
  end_point.set_y(10);
  auto generator = ui_test_utils::DragEventGenerator::CreateForTouch(
      std::make_unique<ui_test_utils::InterpolatedProducer>(
          start_point, end_point, base::TimeDelta::FromMilliseconds(1000)));

  ash::PaginationModel* model = ash::ShellTestApi().GetAppListPaginationModel();
  ASSERT_TRUE(model);
  PageSwitchWaiter waiter(model);
  generator->Wait();
  waiter.Wait();
}
