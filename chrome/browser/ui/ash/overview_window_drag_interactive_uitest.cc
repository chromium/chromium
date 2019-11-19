// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/perf/drag_event_generator.h"
#include "chrome/test/base/perf/performance_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/tween.h"

namespace {

// Wait until the window's state changed to left snapped.
// The window should stay alive, so no need to observer destroying.
class LeftSnapWaiter : public aura::WindowObserver {
 public:
  explicit LeftSnapWaiter(aura::Window* window) : window_(window) {
    window->AddObserver(this);
  }
  ~LeftSnapWaiter() override { window_->RemoveObserver(this); }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key == ash::kWindowStateTypeKey && IsLeftSnapped())
      run_loop_.Quit();
  }

  void Wait() {
    if (!IsLeftSnapped())
      run_loop_.Run();
  }

  bool IsLeftSnapped() {
    return window_->GetProperty(ash::kWindowStateTypeKey) ==
           ash::WindowStateType::kLeftSnapped;
  }

 private:
  aura::Window* window_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(LeftSnapWaiter);
};

}  // namespace

// Test window drag performance in overview mode.
// int: number of windows : 2, 8
// bool: the tab content (about:blank, chrome://newtab)
// bool: touch (true) or mouse(false)
class OverviewWindowDragTest
    : public UIPerformanceTest,
      public testing::WithParamInterface<::testing::tuple<int, bool>> {
 public:
  OverviewWindowDragTest() = default;
  ~OverviewWindowDragTest() override = default;

  // UIPerformanceTest:
  void SetUpOnMainThread() override {
    UIPerformanceTest::SetUpOnMainThread();
    ash::ShellTestApi().SetTabletModeEnabledForTest(true);

    int additional_browsers = std::get<0>(GetParam()) - 1;
    bool blank_page = std::get<1>(GetParam());

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
    return {
        "Ash.Overview.WindowDrag.PresentationTime.TabletMode",
    };
  }

  gfx::Size GetDisplaySize(aura::Window* window) const {
    return display::Screen::GetScreen()->GetDisplayNearestWindow(window).size();
  }

  // Returns the location within the top / left most window.
  gfx::Point GetStartLocation(const gfx::Size& size) const {
    int num_browsers = std::get<0>(GetParam());
    return num_browsers == 2 ? gfx::Point(size.width() / 3, size.height() / 2)
                             : gfx::Point(size.width() / 5, size.height() / 4);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OverviewWindowDragTest);
};

IN_PROC_BROWSER_TEST_P(OverviewWindowDragTest, NormalDrag) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  ui_controls::SendKeyPress(browser_window, ui::VKEY_MEDIA_LAUNCH_APP1,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /*command=*/false);
  ash::ShellTestApi().WaitForOverviewAnimationState(
      ash::OverviewAnimationState::kEnterAnimationComplete);
  gfx::Size display_size = GetDisplaySize(browser_window);
  gfx::Point start_point = GetStartLocation(display_size);
  gfx::Point end_point(start_point);
  end_point.set_x(end_point.x() + display_size.width() / 2);
  auto generator = ui_test_utils::DragEventGenerator::CreateForTouch(
      std::make_unique<ui_test_utils::InterpolatedProducer>(
          start_point, end_point, base::TimeDelta::FromMilliseconds(1000)),
      /*long_press=*/true);
  generator->Wait();
}

// The test is flaky because close notification is not the right singal.
// crbug.com/953355
IN_PROC_BROWSER_TEST_P(OverviewWindowDragTest, DISABLED_DragToClose) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  ui_controls::SendKeyPress(browser_window, ui::VKEY_MEDIA_LAUNCH_APP1,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /*command=*/false);
  ash::ShellTestApi().WaitForOverviewAnimationState(
      ash::OverviewAnimationState::kEnterAnimationComplete);

  gfx::Point start_point = GetStartLocation(GetDisplaySize(browser_window));
  gfx::Point end_point(start_point);
  end_point.set_y(0);
  end_point.set_x(end_point.x() + 10);
  auto generator = ui_test_utils::DragEventGenerator::CreateForTouch(
      std::make_unique<ui_test_utils::InterpolatedProducer>(
          start_point, end_point, base::TimeDelta::FromMilliseconds(500),
          gfx::Tween::EASE_IN_2));
  generator->Wait();

  ui_test_utils::WaitForBrowserToClose(chrome::FindLastActive());
}

// Disable for ChromeOS crbug.com/1021005.
#if defined(OS_CHROMEOS)
#define MAYBE_DragToSnap DISABLED_DragToSnap
#else
#define MAYBE_DragToSnap DragToSnap
#endif
IN_PROC_BROWSER_TEST_P(OverviewWindowDragTest, MAYBE_DragToSnap) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  ui_controls::SendKeyPress(browser_window, ui::VKEY_MEDIA_LAUNCH_APP1,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /*command=*/false);
  ash::ShellTestApi().WaitForOverviewAnimationState(
      ash::OverviewAnimationState::kEnterAnimationComplete);

  gfx::Point start_point = GetStartLocation(GetDisplaySize(browser_window));
  gfx::Point end_point(start_point);
  end_point.set_x(0);
  auto generator = ui_test_utils::DragEventGenerator::CreateForTouch(
      std::make_unique<ui_test_utils::InterpolatedProducer>(
          start_point, end_point, base::TimeDelta::FromMilliseconds(1000)),
      /*long_press=*/true);
  generator->Wait();

  Browser* active = chrome::FindLastActive();
  // Wait for the window to be snapped.
  LeftSnapWaiter(active->window()->GetNativeWindow()).Wait();
}

INSTANTIATE_TEST_SUITE_P(,
                         OverviewWindowDragTest,
                         ::testing::Combine(::testing::Values(2, 8),
                                            /*blank=*/testing::Bool()));
