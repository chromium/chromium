// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/window_state_type.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/ui/ash/ash_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/perf/drag_event_generator.h"
#include "chrome/test/base/perf/performance_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/wm/core/wm_core_switches.h"

namespace {

// A mouse movement producer that generates the position between mulitiple
// points based on the |progress| given to |GetPosition|.  It is guarantted that
// this produces the point specified in |ProducePointsTo| except that
// the DragEventGenerator progressed faster than it can produce points.
// (You can't produce more than 60 points in a seconds, for example).
class MultiPointProducer
    : public ui_test_utils::DragEventGenerator::PointProducer {
 public:
  explicit MultiPointProducer(const gfx::Point& start) : current_(start) {}
  ~MultiPointProducer() override = default;

  // Instructs the produer so that it produces the seuqnece of points from the
  // current point to the next |point| in |duration| time.  |point| will become
  // the new current point.
  void ProducePointsTo(const gfx::Point& point,
                       const base::TimeDelta duration) {
    ASSERT_FALSE(initialized_);

    step_list_.emplace_back(
        Step{0.f, 0.f,
             std::make_unique<ui_test_utils::InterpolatedProducer>(
                 current_, point, duration)});
    current_ = point;
    total_duration_ += duration;
  }

  // PointProducer:
  gfx::Point GetPosition(float progress) override {
    if (!initialized_) {
      initialized_ = true;
      InitSteps();
    }
    EXPECT_FALSE(step_list_.empty());

    // The generating a point given in |ProducePointsTo| is important as it is
    // typically specified to trigger mouse based actions (such as window
    // resize).  Make sure that we generate such points by giving 0.f when the
    // |progress| exceeds the current step.
    if (progress > step_list_[0].to_progress) {
      EXPECT_NE(progress, 1.f)
          << "Failed to produce all points added to the MutiPointProducer";
      EXPECT_GE(step_list_.size(), 2u)
          << "prog=" << progress << ", to=" << step_list_[0].to_progress
          << ", size=" << step_list_.size();
      step_list_.erase(step_list_.begin());
      return step_list_[0].producer->GetPosition(0.f);
    }
    float adjusted = (progress - step_list_[0].from_progress) /
                     (step_list_[0].to_progress - step_list_[0].from_progress);
    return step_list_[0].producer->GetPosition(adjusted);
  }
  const base::TimeDelta GetDuration() const override { return total_duration_; }

 private:
  // Specifies the producer used between |from_progress| and |to_progress|.  The
  // |producer| will receive the 0.f-1.f progress value relative to the range
  // between these two values.
  struct Step {
    float from_progress{0.f};
    float to_progress{0.f};
    std::unique_ptr<ui_test_utils::InterpolatedProducer> producer;
  };

  // Initializes the |from_progress|/|to_progress| for each step
  // based on the |total_duration_|.
  void InitSteps() {
    base::TimeDelta time;
    for (auto& step : step_list_) {
      step.from_progress =
          float{time.InMicroseconds()} / total_duration_.InMicroseconds();
      time += step.producer->GetDuration();
      step.to_progress =
          float{time.InMicroseconds()} / total_duration_.InMicroseconds();
    }
  }

  bool initialized_ = false;
  std::vector<Step> step_list_;
  gfx::Point current_;
  base::TimeDelta total_duration_;

  DISALLOW_COPY_AND_ASSIGN(MultiPointProducer);
};

}  // namespace

// Test window resize performance in clamshell mode.
class WindowResizeTest
    : public UIPerformanceTest,
      public testing::WithParamInterface<::testing::tuple<bool>> {
 public:
  WindowResizeTest() = default;
  ~WindowResizeTest() override = default;

  // UIPerformanceTest:
  void SetUpOnMainThread() override {
    UIPerformanceTest::SetUpOnMainThread();

    use_ntp_ = std::get<0>(GetParam());

    GURL ntp_url("chrome://newtab");
    if (use_ntp_)
      ui_test_utils::NavigateToURL(browser(), ntp_url);

    // Make sure startup tasks won't affect measurement.
    if (base::SysInfo::IsRunningOnChromeOS()) {
      base::RunLoop run_loop;
      base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                            base::TimeDelta::FromSeconds(5));
      run_loop.Run();
    }

    auto* cmd = base::CommandLine::ForCurrentProcess();
    cmd->AppendSwitch(wm::switches::kWindowAnimationsDisabled);
  }

  // UIPerformanceTest:
  std::vector<std::string> GetUMAHistogramNames() const override {
    return {
        "Ash.InteractiveWindowResize.TimeToPresent",
    };
  }

  bool use_ntp() const { return use_ntp_; }

 private:
  bool use_ntp_ = false;

  DISALLOW_COPY_AND_ASSIGN(WindowResizeTest);
};

IN_PROC_BROWSER_TEST_P(WindowResizeTest, Single) {
  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  test::ActivateAndSnapWindow(browser_window,
                              ash::WindowStateType::kLeftSnapped);

  gfx::Rect bounds = browser_window->GetBoundsInScreen();
  gfx::Point start_point = gfx::Point(bounds.right_center());
  start_point.set_y(start_point.y() + 100);
  gfx::Point mid_point(start_point);

  // Move enough amount to produce 60 updates in 1 seconds, but
  // do not exceeds brower's minimum size.
  mid_point.Offset(-60, 0);
  gfx::Point end_point(start_point);
  end_point.Offset(120, 0);

  auto producer = std::make_unique<MultiPointProducer>(start_point);
  producer->ProducePointsTo(mid_point, base::TimeDelta::FromSeconds(1));
  producer->ProducePointsTo(end_point, base::TimeDelta::FromSeconds(1));

  auto generator =
      ui_test_utils::DragEventGenerator::CreateForMouse(std::move(producer));
  generator->Wait();
}

IN_PROC_BROWSER_TEST_P(WindowResizeTest, Multi) {
  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  test::ActivateAndSnapWindow(browser_window,
                              ash::WindowStateType::kLeftSnapped);

  Browser* browser2 = CreateBrowser(browser()->profile());
  if (use_ntp()) {
    GURL ntp_url("chrome://newtab");
    ui_test_utils::NavigateToURL(browser2, ntp_url);
  }

  aura::Window* browser_window2 = browser2->window()->GetNativeWindow();
  // Snap Right
  test::ActivateAndSnapWindow(browser_window2,
                              ash::WindowStateType::kRightSnapped);

  gfx::Rect bounds = browser_window->GetBoundsInScreen();
  gfx::Point start_point = gfx::Point(bounds.right_center());
  ui_controls::SendMouseMove(start_point.x(), start_point.y());
  base::RunLoop run_loop;
  // Wait to trigger resize handle.
  base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                        base::TimeDelta::FromMilliseconds(500));
  run_loop.Run();
  start_point.Offset(0, 50);
  auto producer = std::make_unique<MultiPointProducer>(start_point);
  start_point.Offset(-60, 0);
  producer->ProducePointsTo(start_point, base::TimeDelta::FromSeconds(1));
  start_point.Offset(120, 0);
  producer->ProducePointsTo(start_point, base::TimeDelta::FromSeconds(1));
  auto generator =
      ui_test_utils::DragEventGenerator::CreateForMouse(std::move(producer));
  generator->Wait();
}

INSTANTIATE_TEST_SUITE_P(,
                         WindowResizeTest,
                         ::testing::Combine(/*ntp=*/testing::Bool()));
