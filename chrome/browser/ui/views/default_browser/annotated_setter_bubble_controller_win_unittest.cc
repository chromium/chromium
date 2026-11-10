// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/default_browser/annotated_setter_bubble_controller_win.h"

#include <windows.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/default_browser/win/settings_row_locator_win.h"
#include "chrome/browser/default_browser/win/settings_window_finder_win.h"
#include "chrome/browser/ui/views/default_browser/annotated_setter_bubble.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/win/hwnd_util.h"

namespace default_browser {

namespace {

using Outcome = AnnotatedSetterBubbleController::Outcome;
using FailureReason = SettingsRowLocatorWin::FailureReason;
using OutcomeFuture = base::test::TestFuture<Outcome>;

const HWND kHwnd = reinterpret_cast<HWND>(0x1234);
const HWND kOtherHwnd = reinterpret_cast<HWND>(0x5678);
constexpr base::TimeDelta kFindTimeout = base::Seconds(5);
constexpr gfx::Rect kWindowRect(100, 200, 800, 600);
// Relative to the window's origin.
constexpr gfx::Rect kButtonBounds(20, 100, 200, 40);
constexpr base::TimeDelta kDebounce =
    AnnotatedSetterBubbleControllerWin::kMeasureDebounce;
constexpr base::TimeDelta kRetryDelay =
    AnnotatedSetterBubbleControllerWin::kMeasureRetryDelay;

base::Process OpenCurrentProcess() {
  return base::Process::OpenWithAccess(::GetCurrentProcessId(),
                                       PROCESS_QUERY_LIMITED_INFORMATION);
}

gfx::Rect ScreenToDIPRect(const gfx::Rect& screen_rect) {
  return display::win::GetScreenWin()->ScreenToDIPRect(kHwnd, screen_rect);
}

class FakeSettingsWindowFinder : public SettingsWindowFinderWin {
 public:
  void Start(base::TimeDelta timeout,
             WindowFoundCallback on_found,
             base::OnceClosure on_timeout) override {
    ++start_count_;
    timeout_ = timeout;
    on_found_ = std::move(on_found);
    on_timeout_ = std::move(on_timeout);
  }

  void Stop() override {
    ++stop_count_;
    on_found_.Reset();
    on_timeout_.Reset();
  }

  void StartObservingLocationChanges(
      HWND settings_hwnd,
      WindowResizedCallback on_resized) override {
    observed_hwnd_ = settings_hwnd;
    on_resized_ = std::move(on_resized);
  }

  void StopObservingLocationChanges() override {
    ++stop_observing_count_;
    observed_hwnd_ = nullptr;
    on_resized_.Reset();
  }

  void StartObservingForegroundChanges(
      ForegroundChangedCallback on_foreground_changed) override {
    on_foreground_changed_ = std::move(on_foreground_changed);
  }

  void StopObservingForegroundChanges() override {
    on_foreground_changed_.Reset();
  }

  void TriggerFound(HWND hwnd, base::Process process) {
    ASSERT_TRUE(on_found_);
    std::move(on_found_).Run(hwnd, std::move(process));
  }

  void TriggerTimeout() {
    ASSERT_TRUE(on_timeout_);
    std::move(on_timeout_).Run();
  }

  void TriggerChanged() {
    ASSERT_TRUE(on_resized_);
    on_resized_.Run();
  }

  void TriggerForegroundChanged() {
    ASSERT_TRUE(on_foreground_changed_);
    on_foreground_changed_.Run();
  }

  int start_count() const { return start_count_; }
  int stop_count() const { return stop_count_; }
  int stop_observing_count() const { return stop_observing_count_; }
  base::TimeDelta timeout() const { return timeout_; }
  HWND observed_hwnd() const { return observed_hwnd_; }
  bool is_observing() const { return !on_resized_.is_null(); }
  bool is_observing_foreground() const {
    return !on_foreground_changed_.is_null();
  }

 private:
  WindowFoundCallback on_found_;
  base::OnceClosure on_timeout_;
  WindowResizedCallback on_resized_;
  ForegroundChangedCallback on_foreground_changed_;
  HWND observed_hwnd_ = nullptr;
  base::TimeDelta timeout_;
  int start_count_ = 0;
  int stop_count_ = 0;
  int stop_observing_count_ = 0;
};

// Captures button measurements so a test can answer them by hand. Overriding
// MeasureButton() means no service process is ever launched.
class FakeSettingsRowLocator : public SettingsRowLocatorWin {
 public:
  void MeasureButton(HWND settings_hwnd,
                     base::Process settings_process,
                     ButtonCallback callback) override {
    ++measure_count_;
    last_hwnd_ = settings_hwnd;
    last_pid_ = settings_process.IsValid() ? settings_process.Pid()
                                           : base::kNullProcessId;
    pending_ = std::move(callback);
  }

  void Answer(SettingsRowLocatorWin::ButtonResult result) {
    ASSERT_FALSE(pending_.is_null());
    std::move(pending_).Run(std::move(result));
  }

  int measure_count() const { return measure_count_; }
  bool has_pending() const { return !pending_.is_null(); }
  HWND last_hwnd() const { return last_hwnd_; }
  base::ProcessId last_pid() const { return last_pid_; }

 private:
  ButtonCallback pending_;
  int measure_count_ = 0;
  HWND last_hwnd_ = nullptr;
  base::ProcessId last_pid_ = 0;
};

class FakeWindowInspector
    : public AnnotatedSetterBubbleControllerWin::WindowInspector {
 public:
  std::optional<gfx::Rect> GetWindowRect(HWND hwnd) const override {
    return window_rect_;
  }
  bool IsWindowGone(HWND hwnd, const base::Process& process) const override {
    return window_gone_;
  }
  bool IsWindowMinimized(HWND hwnd) const override { return window_minimized_; }
  bool IsWindowActive(HWND hwnd) const override {
    return hwnd && hwnd == active_hwnd_;
  }

  void set_window_rect(std::optional<gfx::Rect> rect) { window_rect_ = rect; }
  void set_window_gone(bool gone) { window_gone_ = gone; }
  void set_window_minimized(bool minimized) { window_minimized_ = minimized; }
  void set_active_window(HWND hwnd) { active_hwnd_ = hwnd; }

 private:
  std::optional<gfx::Rect> window_rect_ = kWindowRect;
  bool window_gone_ = false;
  bool window_minimized_ = false;
  HWND active_hwnd_ = kHwnd;
};

class AnnotatedSetterBubbleControllerWinTest : public ChromeViewsTestBase {
 protected:
  AnnotatedSetterBubbleControllerWinTest() {
    set_native_widget_type(NativeWidgetType::kDesktop);
  }

  void TearDown() override {
    DestroyController();
    ChromeViewsTestBase::TearDown();
  }

  void Start(bool can_pin_to_taskbar = false) {
    auto finder = std::make_unique<FakeSettingsWindowFinder>();
    finder_ = finder.get();
    auto locator = std::make_unique<FakeSettingsRowLocator>();
    locator_ = locator.get();
    auto window_inspector = std::make_unique<FakeWindowInspector>();
    window_inspector_ = window_inspector.get();

    controller_ = std::make_unique<AnnotatedSetterBubbleControllerWin>(
        kFindTimeout, can_pin_to_taskbar, outcome_.GetCallback(),
        std::move(finder), std::move(locator), std::move(window_inspector));
  }

  void DestroyController() {
    finder_ = nullptr;
    locator_ = nullptr;
    window_inspector_ = nullptr;
    controller_.reset();
  }

  // Starts and delivers the Settings window, which schedules the first
  // measurement.
  void StartWithSettingsWindow(bool can_pin_to_taskbar = false) {
    Start(can_pin_to_taskbar);
    finder_->TriggerFound(kHwnd, OpenCurrentProcess());
  }

  // Lets the debounce run out and answers the measurement it requested.
  void MeasureAndAnswer(SettingsRowLocatorWin::ButtonResult result) {
    AnswerAfter(kDebounce, std::move(result));
  }

  // Lets the retry delay after a failure run out and answers the measurement
  // it requested.
  void RetryAndAnswer(SettingsRowLocatorWin::ButtonResult result) {
    AnswerAfter(kRetryDelay, std::move(result));
  }

  void AnswerAfter(base::TimeDelta delay,
                   SettingsRowLocatorWin::ButtonResult result) {
    task_environment()->FastForwardBy(delay);
    ASSERT_TRUE(locator_);
    ASSERT_TRUE(locator_->has_pending());
    locator_->Answer(std::move(result));
  }

  // Starts, finds the window and lands one measurement, so the bubble shows.
  void StartWithBubbleShown(bool can_pin_to_taskbar = false) {
    StartWithSettingsWindow(can_pin_to_taskbar);
    MeasureAndAnswer(kButtonBounds);
    ASSERT_NE(bubble_widget(), nullptr);
    ASSERT_TRUE(bubble_widget()->IsVisible());
  }

  // Makes `hwnd` the active window and reports the foreground change.
  void ChangeForeground(HWND hwnd) {
    ASSERT_TRUE(window_inspector_);
    ASSERT_TRUE(finder_);
    window_inspector_->set_active_window(hwnd);
    finder_->TriggerForegroundChanged();
  }

  views::Widget* bubble_widget() const {
    return controller_ ? controller_->bubble_widget_for_testing() : nullptr;
  }

  AnnotatedSetterBubble* bubble() const {
    return controller_ ? controller_->bubble_for_testing() : nullptr;
  }

  int measure_count() const { return locator_ ? locator_->measure_count() : 0; }

  OutcomeFuture outcome_;
  std::unique_ptr<AnnotatedSetterBubbleControllerWin> controller_;
  // Owned by `controller_`; declared after it so they are destroyed first.
  raw_ptr<FakeSettingsWindowFinder> finder_ = nullptr;
  raw_ptr<FakeSettingsRowLocator> locator_ = nullptr;
  raw_ptr<FakeWindowInspector> window_inspector_ = nullptr;
};

TEST_F(AnnotatedSetterBubbleControllerWinTest, StartLooksForSettings) {
  Start();

  EXPECT_EQ(finder_->start_count(), 1);
  EXPECT_EQ(finder_->timeout(), kFindTimeout);
  EXPECT_EQ(bubble_widget(), nullptr);
  EXPECT_FALSE(outcome_.IsReady());
}

TEST_F(AnnotatedSetterBubbleControllerWinTest, FindTimeoutEndsGuidance) {
  Start();

  finder_->TriggerTimeout();

  EXPECT_EQ(outcome_.Get(), Outcome::kSettingsWindowNotFound);
}

TEST_F(AnnotatedSetterBubbleControllerWinTest,
       FoundWindowIsObservedAndMeasuredAfterTheDebounce) {
  StartWithSettingsWindow();

  EXPECT_EQ(finder_->observed_hwnd(), kHwnd);
  EXPECT_TRUE(finder_->is_observing_foreground());
  EXPECT_EQ(bubble_widget(), nullptr);
  EXPECT_EQ(measure_count(), 0);

  task_environment()->FastForwardBy(kDebounce - base::Milliseconds(1));
  EXPECT_EQ(measure_count(), 0);

  task_environment()->FastForwardBy(base::Milliseconds(1));
  ASSERT_EQ(measure_count(), 1);
  EXPECT_EQ(locator_->last_hwnd(), kHwnd);
  EXPECT_EQ(locator_->last_pid(), ::GetCurrentProcessId());
  EXPECT_EQ(bubble_widget(), nullptr);
}

TEST_F(AnnotatedSetterBubbleControllerWinTest,
       BubbleIsShownAtTheButtonOnceMeasured) {
  StartWithSettingsWindow(/*can_pin_to_taskbar=*/false);

  MeasureAndAnswer(kButtonBounds);

  ASSERT_NE(bubble_widget(), nullptr);
  EXPECT_TRUE(bubble_widget()->IsVisible());
  EXPECT_EQ(bubble()->GetAnchorRect(),
            ScreenToDIPRect(kButtonBounds + kWindowRect.OffsetFromOrigin()));
  EXPECT_EQ(bubble()->GetContentsView()->children().size(), 2u);
  EXPECT_FALSE(outcome_.IsReady());
}

TEST_F(AnnotatedSetterBubbleControllerWinTest,
       CanPinToTaskbarAddsTaskbarPinStep) {
  StartWithBubbleShown(/*can_pin_to_taskbar=*/true);

  EXPECT_EQ(bubble()->GetContentsView()->children().size(), 3u);
}

TEST_F(AnnotatedSetterBubbleControllerWinTest,
       BubbleFollowsAMoveWithoutRemeasuring) {
  StartWithBubbleShown();
  views::Widget* const initial_widget = bubble_widget();

  const gfx::Rect moved(kWindowRect.origin() + gfx::Vector2d(50, -30),
                        kWindowRect.size());
  window_inspector_->set_window_rect(moved);
  finder_->TriggerChanged();

  EXPECT_EQ(bubble_widget(), initial_widget);
  EXPECT_TRUE(bubble_widget()->IsVisible());
  EXPECT_EQ(bubble()->GetAnchorRect(),
            ScreenToDIPRect(kButtonBounds + moved.OffsetFromOrigin()));
  EXPECT_EQ(measure_count(), 1);
}

TEST_F(AnnotatedSetterBubbleControllerWinTest, ResizeHidesAndRemeasures) {
  StartWithBubbleShown();

  const gfx::Rect resized(kWindowRect.origin(), gfx::Size(700, 500));
  window_inspector_->set_window_rect(resized);
  finder_->TriggerChanged();

  ASSERT_NE(bubble_widget(), nullptr);
  EXPECT_FALSE(bubble_widget()->IsVisible());
  EXPECT_EQ(measure_count(), 1);

  const gfx::Rect new_bounds(30, 120, 200, 40);
  MeasureAndAnswer(new_bounds);

  EXPECT_EQ(measure_count(), 2);
  EXPECT_TRUE(bubble_widget()->IsVisible());
  EXPECT_EQ(bubble()->GetAnchorRect(),
            ScreenToDIPRect(new_bounds + resized.OffsetFromOrigin()));
}

TEST_F(AnnotatedSetterBubbleControllerWinTest,
       RepeatedChangesAtOneSizeMeasureOnce) {
  StartWithSettingsWindow();

  for (int i = 0; i < 3; ++i) {
    task_environment()->FastForwardBy(kDebounce / 2);
    finder_->TriggerChanged();
  }
  task_environment()->FastForwardBy(kDebounce);

  EXPECT_EQ(measure_count(), 1);
}

TEST_F(AnnotatedSetterBubbleControllerWinTest, ReplyForAnOldSizeIsDropped) {
  StartWithSettingsWindow();
  task_environment()->FastForwardBy(kDebounce);
  ASSERT_EQ(measure_count(), 1);

  const gfx::Rect resized(kWindowRect.origin(), gfx::Size(700, 500));
  window_inspector_->set_window_rect(resized);
  finder_->TriggerChanged();

  locator_->Answer(kButtonBounds);

  EXPECT_EQ(bubble_widget(), nullptr);
  EXPECT_FALSE(outcome_.IsReady());

  // The new size is measured once its debounce runs out, and the stale reply
  // did not use up the failure budget.
  MeasureAndAnswer(base::unexpected(FailureReason::kNotFound));
  for (int i = 1;
       i <
       AnnotatedSetterBubbleControllerWin::kMaxConsecutiveMeasurementFailures -
           1;
       ++i) {
    RetryAndAnswer(base::unexpected(FailureReason::kNotFound));
  }
  EXPECT_FALSE(outcome_.IsReady());
  EXPECT_EQ(measure_count(), 1 +
                                 AnnotatedSetterBubbleControllerWin::
                                     kMaxConsecutiveMeasurementFailures -
                                 1);
}

TEST_F(AnnotatedSetterBubbleControllerWinTest, SupersededIsNotAFailure) {
  StartWithSettingsWindow();

  for (int i = 0;
       i <
       AnnotatedSetterBubbleControllerWin::kMaxConsecutiveMeasurementFailures +
           1;
       ++i) {
    task_environment()->FastForwardBy(kDebounce);
    ASSERT_TRUE(locator_->has_pending());
    locator_->Answer(base::unexpected(FailureReason::kSuperseded));
    // Nothing is rescheduled for a superseded reply; ask again as a resize
    // would.
    window_inspector_->set_window_rect(
        gfx::Rect(kWindowRect.origin(), gfx::Size(700 + i, 500)));
    finder_->TriggerChanged();
  }

  EXPECT_FALSE(outcome_.IsReady());
}

TEST_F(AnnotatedSetterBubbleControllerWinTest,
       FailureIsRetriedUntilTheBudgetRunsOut) {
  StartWithSettingsWindow();

  MeasureAndAnswer(base::unexpected(FailureReason::kNotFound));
  EXPECT_FALSE(outcome_.IsReady());
  EXPECT_EQ(measure_count(), 1);
  for (int i = 2;
       i <
       AnnotatedSetterBubbleControllerWin::kMaxConsecutiveMeasurementFailures;
       ++i) {
    RetryAndAnswer(base::unexpected(FailureReason::kNotFound));
    EXPECT_FALSE(outcome_.IsReady());
    EXPECT_EQ(measure_count(), i);
  }

  RetryAndAnswer(base::unexpected(FailureReason::kTimedOut));

  EXPECT_EQ(outcome_.Get(), Outcome::kButtonNotFound);
  EXPECT_EQ(bubble_widget(), nullptr);
}

// A failure usually means the page is still loading, so the retry waits
// longer than the resize debounce.
TEST_F(AnnotatedSetterBubbleControllerWinTest,
       FailureIsRetriedAfterTheRetryDelay) {
  static_assert(kRetryDelay > kDebounce);
  StartWithSettingsWindow();
  MeasureAndAnswer(base::unexpected(FailureReason::kNotFound));
  ASSERT_EQ(measure_count(), 1);

  task_environment()->FastForwardBy(kRetryDelay - base::Milliseconds(1));
  EXPECT_EQ(measure_count(), 1);

  task_environment()->FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(measure_count(), 2);
}

TEST_F(AnnotatedSetterBubbleControllerWinTest,
       ResizeWhileWaitingToRetryMeasuresAfterTheDebounce) {
  StartWithSettingsWindow();
  MeasureAndAnswer(base::unexpected(FailureReason::kNotFound));
  ASSERT_EQ(measure_count(), 1);

  window_inspector_->set_window_rect(
      gfx::Rect(kWindowRect.origin(), gfx::Size(700, 500)));
  finder_->TriggerChanged();

  task_environment()->FastForwardBy(kDebounce);
  EXPECT_EQ(measure_count(), 2);

  // The retry was replaced rather than run in addition.
  locator_->Answer(kButtonBounds);
  task_environment()->FastForwardBy(kRetryDelay);
  EXPECT_EQ(measure_count(), 2);
  ASSERT_NE(bubble_widget(), nullptr);
  EXPECT_TRUE(bubble_widget()->IsVisible());
}

TEST_F(AnnotatedSetterBubbleControllerWinTest, SuccessResetsTheFailureBudget) {
  StartWithSettingsWindow();

  MeasureAndAnswer(base::unexpected(FailureReason::kNotFound));
  RetryAndAnswer(base::unexpected(FailureReason::kNotFound));
  RetryAndAnswer(kButtonBounds);
  ASSERT_NE(bubble_widget(), nullptr);
  EXPECT_TRUE(bubble_widget()->IsVisible());

  // Two more failures after the success do not add up to three in a row.
  window_inspector_->set_window_rect(
      gfx::Rect(kWindowRect.origin(), gfx::Size(700, 500)));
  finder_->TriggerChanged();
  MeasureAndAnswer(base::unexpected(FailureReason::kNotFound));
  RetryAndAnswer(base::unexpected(FailureReason::kNotFound));

  EXPECT_FALSE(outcome_.IsReady());
  EXPECT_EQ(measure_count(), 5);
}

TEST_F(AnnotatedSetterBubbleControllerWinTest, WindowGoneReplyEndsGuidance) {
  StartWithSettingsWindow();

  MeasureAndAnswer(base::unexpected(FailureReason::kWindowGone));

  EXPECT_EQ(outcome_.Get(), Outcome::kSettingsWindowClosed);
}

TEST_F(AnnotatedSetterBubbleControllerWinTest, WindowGoneOnChangeEndsGuidance) {
  StartWithBubbleShown();

  window_inspector_->set_window_gone(true);
  finder_->TriggerChanged();

  EXPECT_EQ(outcome_.Get(), Outcome::kSettingsWindowClosed);
  EXPECT_EQ(bubble_widget(), nullptr);
  EXPECT_FALSE(finder_->is_observing());
}

// A reused handle is never sent to UIA: querying a Chrome window would turn
// on Chrome's accessibility mode.
TEST_F(AnnotatedSetterBubbleControllerWinTest,
       WindowGoneBeforeMeasuringEndsGuidanceWithoutMeasuring) {
  StartWithSettingsWindow();

  window_inspector_->set_window_gone(true);
  task_environment()->FastForwardBy(kDebounce);

  EXPECT_EQ(outcome_.Get(), Outcome::kSettingsWindowClosed);
  EXPECT_EQ(measure_count(), 0);
}

TEST_F(AnnotatedSetterBubbleControllerWinTest,
       MinimizedHidesTheBubbleAndRestoredShowsItAgain) {
  StartWithBubbleShown();

  window_inspector_->set_window_minimized(true);
  finder_->TriggerChanged();

  EXPECT_FALSE(bubble_widget()->IsVisible());

  window_inspector_->set_window_minimized(false);
  finder_->TriggerChanged();

  EXPECT_TRUE(bubble_widget()->IsVisible());
  EXPECT_EQ(bubble()->GetAnchorRect(),
            ScreenToDIPRect(kButtonBounds + kWindowRect.OffsetFromOrigin()));
  EXPECT_EQ(measure_count(), 1);
  EXPECT_FALSE(outcome_.IsReady());
}

TEST_F(AnnotatedSetterBubbleControllerWinTest,
       MinimizedWhenTheDebounceFiresMeasuresOnceRestored) {
  StartWithSettingsWindow();

  window_inspector_->set_window_minimized(true);
  task_environment()->FastForwardBy(kDebounce);
  EXPECT_EQ(measure_count(), 0);

  window_inspector_->set_window_minimized(false);
  finder_->TriggerChanged();
  EXPECT_EQ(measure_count(), 0);

  task_environment()->FastForwardBy(kDebounce);
  EXPECT_EQ(measure_count(), 1);
}

TEST_F(AnnotatedSetterBubbleControllerWinTest,
       UnavailableWindowRectHidesTheBubble) {
  StartWithBubbleShown();

  window_inspector_->set_window_rect(std::nullopt);
  finder_->TriggerChanged();

  EXPECT_FALSE(bubble_widget()->IsVisible());
  EXPECT_FALSE(outcome_.IsReady());
}

TEST_F(AnnotatedSetterBubbleControllerWinTest,
       UnfocusedHidesTheBubbleAndRefocusedShowsItAgain) {
  StartWithBubbleShown();

  ChangeForeground(kOtherHwnd);
  EXPECT_FALSE(bubble_widget()->IsVisible());
  EXPECT_FALSE(outcome_.IsReady());

  ChangeForeground(kHwnd);
  EXPECT_TRUE(bubble_widget()->IsVisible());
  EXPECT_EQ(bubble()->GetAnchorRect(),
            ScreenToDIPRect(kButtonBounds + kWindowRect.OffsetFromOrigin()));
  EXPECT_EQ(measure_count(), 1);
}

TEST_F(AnnotatedSetterBubbleControllerWinTest,
       UnfocusedWhenMeasuredWaitsForFocusBeforeShowing) {
  Start();
  window_inspector_->set_active_window(kOtherHwnd);
  finder_->TriggerFound(kHwnd, OpenCurrentProcess());
  MeasureAndAnswer(kButtonBounds);

  EXPECT_EQ(bubble_widget(), nullptr);
  EXPECT_FALSE(outcome_.IsReady());

  ChangeForeground(kHwnd);
  ASSERT_NE(bubble_widget(), nullptr);
  EXPECT_TRUE(bubble_widget()->IsVisible());
  EXPECT_EQ(bubble()->GetAnchorRect(),
            ScreenToDIPRect(kButtonBounds + kWindowRect.OffsetFromOrigin()));
}

TEST_F(AnnotatedSetterBubbleControllerWinTest,
       FocusingTheBubbleKeepsItVisibleUntilFocusMovesElsewhere) {
  StartWithBubbleShown();
  HWND bubble_hwnd = views::HWNDForWidget(bubble_widget());
  ASSERT_NE(bubble_hwnd, nullptr);

  ChangeForeground(bubble_hwnd);
  EXPECT_TRUE(bubble_widget()->IsVisible());

  ChangeForeground(kOtherHwnd);
  EXPECT_FALSE(bubble_widget()->IsVisible());
}

TEST_F(AnnotatedSetterBubbleControllerWinTest, ClosingTheBubbleEndsGuidance) {
  StartWithBubbleShown();

  bubble_widget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);

  EXPECT_EQ(outcome_.Get(), Outcome::kDismissed);
  EXPECT_EQ(bubble_widget(), nullptr);
  EXPECT_FALSE(finder_->is_observing());
  EXPECT_FALSE(finder_->is_observing_foreground());
}

TEST_F(AnnotatedSetterBubbleControllerWinTest, NoReplyAfterTheGuidanceEnded) {
  StartWithSettingsWindow();
  task_environment()->FastForwardBy(kDebounce);
  ASSERT_TRUE(locator_->has_pending());

  window_inspector_->set_window_gone(true);
  finder_->TriggerChanged();
  ASSERT_EQ(outcome_.Get(), Outcome::kSettingsWindowClosed);

  // The reply's weak pointer was invalidated.
  locator_->Answer(kButtonBounds);

  EXPECT_EQ(bubble_widget(), nullptr);
}

TEST_F(AnnotatedSetterBubbleControllerWinTest,
       DestructionStopsGuidanceWithoutRunningTheCompletion) {
  StartWithBubbleShown();
  views::test::WidgetDestroyedWaiter widget_destroyed(bubble_widget());

  DestroyController();
  widget_destroyed.Wait();

  EXPECT_FALSE(outcome_.IsReady());
}

TEST_F(AnnotatedSetterBubbleControllerWinTest,
       DestructionInsideCompletionCallbackIsSafe) {
  auto finder = std::make_unique<FakeSettingsWindowFinder>();
  FakeSettingsWindowFinder* raw_finder = finder.get();
  auto locator = std::make_unique<FakeSettingsRowLocator>();
  FakeSettingsRowLocator* raw_locator = locator.get();
  auto window_inspector = std::make_unique<FakeWindowInspector>();

  std::optional<Outcome> reported_outcome;
  std::unique_ptr<AnnotatedSetterBubbleControllerWin> controller;
  controller = std::make_unique<AnnotatedSetterBubbleControllerWin>(
      kFindTimeout, /*can_pin_to_taskbar=*/false,
      base::BindLambdaForTesting([&](Outcome outcome) {
        reported_outcome = outcome;
        controller.reset();
      }),
      std::move(finder), std::move(locator), std::move(window_inspector));

  raw_finder->TriggerFound(kHwnd, OpenCurrentProcess());
  task_environment()->FastForwardBy(kDebounce);
  ASSERT_TRUE(raw_locator->has_pending());
  raw_locator->Answer(kButtonBounds);

  views::Widget* widget = controller->bubble_widget_for_testing();
  ASSERT_NE(widget, nullptr);
  widget->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);

  EXPECT_EQ(reported_outcome, Outcome::kDismissed);
  EXPECT_EQ(controller, nullptr);
}

}  // namespace

}  // namespace default_browser
