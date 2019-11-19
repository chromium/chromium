// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_state_type.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/ui/ash/ash_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/perf/drag_event_generator.h"
#include "chrome/test/base/perf/performance_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/aura/window.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/wm/core/wm_core_switches.h"

namespace {

class SplitViewTest
    : public UIPerformanceTest,
      public testing::WithParamInterface<::testing::tuple<bool, bool>> {
 public:
  SplitViewTest() = default;
  ~SplitViewTest() override = default;

  // UIPerformanceTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // We're not interested in window transition animation in this test,
    // so just disable it.
    command_line->AppendSwitch(wm::switches::kWindowAnimationsDisabled);
  }
  void SetUpOnMainThread() override {
    UIPerformanceTest::SetUpOnMainThread();

    use_ntp_ = std::get<0>(GetParam());
    use_touch_ = std::get<1>(GetParam());

    if (use_ntp_)
      ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  }

  bool use_touch() const { return use_touch_; }

  void SetUMAToObserve(std::string name) { uma_name_ = std::move(name); }

  Browser* CreateBrowserMaybeWithNtp() {
    Browser* new_browser = CreateBrowser(browser()->profile());
    if (use_ntp_)
      ui_test_utils::NavigateToURL(new_browser,
                                   GURL(chrome::kChromeUINewTabURL));
    return new_browser;
  }

  void WaitForWarmup() {
    // If running on device, wait a bit so that the display is actually powered
    // on.
    // TODO: this should be better factored.
    base::TimeDelta warmup = base::TimeDelta::FromSeconds(
        base::SysInfo::IsRunningOnChromeOS() ? 5 : 0);
    // Give ntp at least 1 seconds to be fully resized.
    if (use_ntp_ && warmup.is_zero())
      warmup += base::TimeDelta::FromSeconds(1);

    if (!warmup.is_zero()) {
      base::RunLoop run_loop;
      base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(), warmup);
      run_loop.Run();
    }
  }

 private:
  std::vector<std::string> GetUMAHistogramNames() const override {
    return {uma_name_};
  }

  bool use_ntp_;
  bool use_touch_;
  std::string uma_name_;

  DISALLOW_COPY_AND_ASSIGN(SplitViewTest);
};

IN_PROC_BROWSER_TEST_P(SplitViewTest, ResizeTwoWindows) {
  SetUMAToObserve(
      "Ash.SplitViewResize.PresentationTime.TabletMode.MultiWindow");

  // This test is intended to gauge performance of resizing windows while in
  // tablet mode split view. It does the following:
  // . creates two browser windows.
  // . snaps one to the left, one to the right.
  // . enters tablet mode.
  // . drags the resizer, which triggers resizing both browsers.

  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  test::ActivateAndSnapWindow(browser_window,
                              ash::WindowStateType::kLeftSnapped);

  Browser* browser2 = CreateBrowserMaybeWithNtp();
  aura::Window* browser2_window = browser2->window()->GetNativeWindow();
  test::ActivateAndSnapWindow(browser2_window,
                              ash::WindowStateType::kRightSnapped);
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  WaitForWarmup();

  const gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().size();
  const gfx::Point start_position(gfx::Rect(display_size).CenterPoint());
  TRACE_EVENT_ASYNC_BEGIN0("ui", "Interaction.ui_WindowResize", this);
  gfx::Point end_position(start_position);
  end_position.set_x(end_position.x() - 60);
  auto generator =
      use_touch() ? ui_test_utils::DragEventGenerator::CreateForTouch(
                        std::make_unique<ui_test_utils::InterpolatedProducer>(
                            start_position, end_position,
                            base::TimeDelta::FromMilliseconds(1000)))
                  : ui_test_utils::DragEventGenerator::CreateForMouse(
                        std::make_unique<ui_test_utils::InterpolatedProducer>(
                            start_position, end_position,
                            base::TimeDelta::FromMilliseconds(1000)));
  generator->Wait();
  TRACE_EVENT_ASYNC_END0("ui", "Interaction.ui_WindowResize", this);

  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
}

IN_PROC_BROWSER_TEST_P(SplitViewTest, ResizeWithOverview) {
  SetUMAToObserve(
      "Ash.SplitViewResize.PresentationTime.TabletMode.WithOverview");

  Browser* browser2 = CreateBrowserMaybeWithNtp();

  aura::Window* browser2_window = browser2->window()->GetNativeWindow();
  test::ActivateAndSnapWindow(browser2_window,
                              ash::WindowStateType::kRightSnapped);

  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  ash::ShellTestApi().WaitForOverviewAnimationState(
      ash::OverviewAnimationState::kEnterAnimationComplete);

  WaitForWarmup();

  const gfx::Point start_position =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().CenterPoint();
  TRACE_EVENT_ASYNC_BEGIN0("ui", "Interaction.ui_WindowResize", this);
  gfx::Point end_position(start_position);
  end_position.set_x(end_position.x() - 60);
  auto generator =
      use_touch() ? ui_test_utils::DragEventGenerator::CreateForTouch(
                        std::make_unique<ui_test_utils::InterpolatedProducer>(
                            start_position, end_position,
                            base::TimeDelta::FromMilliseconds(1000)))
                  : ui_test_utils::DragEventGenerator::CreateForMouse(
                        std::make_unique<ui_test_utils::InterpolatedProducer>(
                            start_position, end_position,
                            base::TimeDelta::FromMilliseconds(1000)));
  generator->Wait();
  TRACE_EVENT_ASYNC_END0("ui", "Interaction.ui_WindowResize", this);

  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
}

INSTANTIATE_TEST_SUITE_P(,
                         SplitViewTest,
                         ::testing::Combine(/*ntp=*/testing::Bool(),
                                            /*touch=*/testing::Bool()));

}  // namespace
