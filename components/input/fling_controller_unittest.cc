// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/fling_controller.h"

#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/fling_booster.h"
#include "ui/events/gestures/physics_based_fling_curve.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseWheelEvent;
using ui::PhysicsBasedFlingCurve;

namespace {
constexpr double kFrameDelta = 1000.0 / 60.0;
}  // namespace

namespace input {

class FakeFlingController : public FlingController {
 public:
  FakeFlingController(FlingControllerEventSenderClient* event_sender_client,
                      FlingControllerSchedulerClient* scheduler_client,
                      const Config& config)
      : FlingController(event_sender_client, scheduler_client, config) {}
};

class FlingControllerTest : public FlingControllerEventSenderClient,
                            public FlingControllerSchedulerClient,
                            public testing::TestWithParam<bool> {
 public:
  // testing::Test
  FlingControllerTest()
      : needs_begin_frame_for_fling_progress_(GetParam()),
        task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  FlingControllerTest(const FlingControllerTest&) = delete;
  FlingControllerTest& operator=(const FlingControllerTest&) = delete;

  ~FlingControllerTest() override {}

  void SetUp() override {
    fling_controller_ = std::make_unique<FakeFlingController>(
        this, this, FlingController::Config());
    fling_controller_->set_clock_for_testing(&mock_clock_);
    AdvanceTime();
  }

  // FlingControllerEventSenderClient
  void SendGeneratedWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) override {
    wheel_event_count_++;
    last_sent_wheel_ = wheel_event.event;
    first_wheel_event_sent_ = true;

    if (wheel_event.event.momentum_phase == WebMouseWheelEvent::kPhaseEnded)
      first_wheel_event_sent_ = false;
  }
  void SendGeneratedGestureScrollEvents(
      const GestureEventWithLatencyInfo& gesture_event) override {
    fling_controller_->ObserveAndMaybeConsumeGestureEvent(gesture_event);
    sent_scroll_gesture_count_++;
    last_sent_gesture_ = gesture_event.event;
  }

  gfx::Size GetRootWidgetViewportSize() override {
    return gfx::Size(1920, 1080);
  }

  // FlingControllerSchedulerClient
  void ScheduleFlingProgress(
      base::WeakPtr<FlingController> fling_controller) override {
    DCHECK(!scheduled_next_fling_progress_);
    scheduled_next_fling_progress_ = true;
  }
  void DidStopFlingingOnBrowser(
      base::WeakPtr<FlingController> fling_controller) override {
    notified_client_after_fling_stop_ = true;
  }
  bool NeedsBeginFrameForFlingProgress() override {
    return needs_begin_frame_for_fling_progress_;
  }
  bool ShouldUseMobileFlingCurve() override {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    return true;
#else
    return false;
#endif
  }
  gfx::Vector2dF GetPixelsPerInch(
      const gfx::PointF& position_in_screen) override {
    return gfx::Vector2dF(kDefaultPixelsPerInch, kDefaultPixelsPerInch);
  }

  void SimulateFlingStart(blink::WebGestureDevice source_device,
                          const gfx::Vector2dF& velocity,
                          bool wait_before_processing = true) {
    scheduled_next_fling_progress_ = false;
    sent_scroll_gesture_count_ = 0;
    WebGestureEvent fling_start(WebInputEvent::Type::kGestureFlingStart, 0,
                                NowTicks(), source_device);
    fling_start.data.fling_start.velocity_x = velocity.x();
    fling_start.data.fling_start.velocity_y = velocity.y();
    GestureEventWithLatencyInfo fling_start_with_latency(fling_start);
    if (wait_before_processing) {
      // Wait for up to one frame before processing the event.
      AdvanceTime(base::RandInt(0, static_cast<int>(kFrameDelta)));
    }
    fling_controller_->ObserveAndMaybeConsumeGestureEvent(
        fling_start_with_latency);
  }

  void SimulateScrollBegin(blink::WebGestureDevice source_device,
                           const gfx::Vector2dF& delta) {
    WebGestureEvent scroll_begin(WebInputEvent::Type::kGestureScrollBegin, 0,
                                 NowTicks(), source_device);
    scroll_begin.data.scroll_begin.delta_x_hint = delta.x();
    scroll_begin.data.scroll_begin.delta_y_hint = delta.y();
    scroll_begin.data.scroll_begin.inertial_phase =
        WebGestureEvent::InertialPhaseState::kNonMomentum;
    scroll_begin.data.scroll_begin.delta_hint_units =
        ui::ScrollGranularity::kScrollByPrecisePixel;
    GestureEventWithLatencyInfo scroll_begin_with_latency(scroll_begin);

    fling_controller_->ObserveAndMaybeConsumeGestureEvent(
        scroll_begin_with_latency);
  }

  void SimulateScrollUpdate(blink::WebGestureDevice source_device,
                            const gfx::Vector2dF& delta) {
    WebGestureEvent scroll_update(WebInputEvent::Type::kGestureScrollUpdate, 0,
                                  NowTicks(), source_device);
    scroll_update.data.scroll_update.delta_x = delta.x();
    scroll_update.data.scroll_update.delta_y = delta.y();
    scroll_update.data.scroll_update.inertial_phase =
        WebGestureEvent::InertialPhaseState::kNonMomentum;
    scroll_update.data.scroll_update.delta_units =
        ui::ScrollGranularity::kScrollByPrecisePixel;
    GestureEventWithLatencyInfo scroll_update_with_latency(
        scroll_update);

    fling_controller_->ObserveAndMaybeConsumeGestureEvent(
        scroll_update_with_latency);
  }

  void SimulateFlingCancel(blink::WebGestureDevice source_device) {
    notified_client_after_fling_stop_ = false;
    WebGestureEvent fling_cancel(WebInputEvent::Type::kGestureFlingCancel, 0,
                                 NowTicks(), source_device);
    // autoscroll fling cancel doesn't allow fling boosting.
    if (source_device == blink::WebGestureDevice::kSyntheticAutoscroll)
      fling_cancel.data.fling_cancel.prevent_boosting = true;
    GestureEventWithLatencyInfo fling_cancel_with_latency(fling_cancel);
    fling_controller_->ObserveAndMaybeConsumeGestureEvent(
        fling_cancel_with_latency);
  }

  void ProgressFling(base::TimeTicks current_time) {
    DCHECK(scheduled_next_fling_progress_);
    scheduled_next_fling_progress_ = false;
    fling_controller_->ProgressFling(current_time);
  }

  bool FlingInProgress() { return fling_controller_->fling_in_progress(); }

  void AdvanceTime(double time_delta_ms = kFrameDelta) {
    mock_clock_.Advance(base::Milliseconds(time_delta_ms));
  }

  base::TimeTicks NowTicks() const { return mock_clock_.NowTicks(); }

  float CompleteFlingAndAccumulateScrollDelta() {
    float total_scroll_delta = 0.f;

    // Sometimes SendGeneratedGestureScrollEvents is not run because fling is
    // not advanced. This is due to the first |FlingScheduler::OnAnimationStep|
    // call having the time of the last frame before AddAnimationObserver.
    // Please see comment in |FlingController::ProgressFling|. This leaves the
    // last_sent_gesture as a GSE. We therefore don't accrue delta in this case
    if (last_sent_gesture_.GetType() !=
        WebInputEvent::Type::kGestureScrollEnd) {
      DCHECK(last_sent_gesture_.GetType() ==
             WebInputEvent::Type::kGestureScrollUpdate);
      total_scroll_delta += last_sent_gesture_.data.scroll_update.delta_x;
    }

    while (true) {
      AdvanceTime();
      ProgressFling(NowTicks());
      if (last_sent_gesture_.GetType() ==
          WebInputEvent::Type::kGestureScrollEnd) {
        break;
      } else {
        DCHECK(last_sent_gesture_.GetType() ==
               WebInputEvent::Type::kGestureScrollUpdate);
        total_scroll_delta += last_sent_gesture_.data.scroll_update.delta_x;
      }
    }

    return total_scroll_delta;
  }

 protected:
  std::unique_ptr<FakeFlingController> fling_controller_;
  int wheel_event_count_ = 0;
  WebMouseWheelEvent last_sent_wheel_;
  WebGestureEvent last_sent_gesture_;
  bool scheduled_next_fling_progress_ = false;
  bool notified_client_after_fling_stop_ = false;
  bool first_wheel_event_sent_ = false;
  int sent_scroll_gesture_count_ = 0;

 private:
  base::SimpleTestTickClock mock_clock_;

  // This determines whether the platform ticks fling animations using
  // SetNeedsBeginFrame (i.e. WebView). If true, we should avoid calling
  // ProgressFling immediately after a FlingStart since this will match the
  // behavior in FlingController::ProcessGestureFlingStart. See
  // https://crrev.com/c/1181521.
  bool needs_begin_frame_for_fling_progress_;
  base::test::TaskEnvironment task_environment_;
};

INSTANTIATE_TEST_SUITE_P(All, FlingControllerTest, testing::Bool());

TEST_P(FlingControllerTest,
       ControllerSendsWheelEndOnTouchpadFlingWithZeroVelocity) {
  SimulateFlingStart(blink::WebGestureDevice::kTouchpad, gfx::Vector2dF());
  // The controller doesn't start a fling and sends a wheel end event
  // immediately.
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, last_sent_wheel_.momentum_phase);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_x);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_y);
}

TEST_P(FlingControllerTest,
       ControllerSendsGSEOnTouchscreenFlingWithZeroVelocity) {
  SimulateFlingStart(blink::WebGestureDevice::kTouchscreen, gfx::Vector2dF());
  // The controller doesn't start a fling and sends a GSE immediately.
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollEnd,
            last_sent_gesture_.GetType());
}

TEST_P(FlingControllerTest, ControllerHandlesTouchpadGestureFling) {
  SimulateFlingStart(blink::WebGestureDevice::kTouchpad,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  // Processing GFS will send the first fling progress event if the time delta
  // between the timestamp of the GFS and the time that ProcessGestureFlingStart
  // is called is large enough.
  bool process_GFS_sent_first_event = first_wheel_event_sent_;

  AdvanceTime();
  ProgressFling(NowTicks());

  if (!process_GFS_sent_first_event) {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, last_sent_wheel_.momentum_phase);
  } else {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged,
              last_sent_wheel_.momentum_phase);
  }
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  // The rest of the wheel events must have momentum_phase == KPhaseChanged.
  AdvanceTime();
  ProgressFling(NowTicks());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged, last_sent_wheel_.momentum_phase);
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  // Now cancel the fling.
  SimulateFlingCancel(blink::WebGestureDevice::kTouchpad);
  EXPECT_FALSE(FlingInProgress());
}

// Ensure that the start time of a fling is measured from the last received
// GSU. This ensures that the first progress fling during FlingStart should
// send significant delta. If we're using the FlingStart as the start time, we
// would send none or very little delta.
TEST_P(FlingControllerTest, FlingStartsAtLastScrollUpdate) {
  SimulateScrollUpdate(blink::WebGestureDevice::kTouchscreen,
                       gfx::Vector2dF(1000, 0));
  double time_to_advance_ms = 30.0;
  AdvanceTime(time_to_advance_ms);
  SimulateFlingStart(blink::WebGestureDevice::kTouchscreen,
                     gfx::Vector2dF(1000, 0), /*wait_before_processing=*/false);
  EXPECT_TRUE(FlingInProgress());

  if (NeedsBeginFrameForFlingProgress())
    ProgressFling(NowTicks());

  // We haven't advanced time since the FlingStart. Ensure we still send a
  // significant amount of delta (~0.030sec * 1000pixels/sec) since we should
  // be measuring the time since the last GSU.
  EXPECT_EQ(1, sent_scroll_gesture_count_);
  ASSERT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            last_sent_gesture_.GetType());
  EXPECT_NEAR(last_sent_gesture_.data.scroll_update.delta_x, 30.0, 5);
}

// Tests that when a fling is interrupted (e.g. by having reached the end of
// the content), a subsequent fling isn't boosted. An example here would be an
// infinite scroller that loads more content after hitting the scroll extent.
TEST_P(FlingControllerTest, InterruptedFlingIsntBoosted) {
  double time_to_advance_ms = 8.0;

  // Start an ordinary fling.
  {
    AdvanceTime(time_to_advance_ms);
    SimulateScrollBegin(blink::WebGestureDevice::kTouchscreen,
                        gfx::Vector2dF(10, 0));
    SimulateScrollUpdate(blink::WebGestureDevice::kTouchscreen,
                         gfx::Vector2dF(10, 0));
    SimulateFlingStart(blink::WebGestureDevice::kTouchscreen,
                       gfx::Vector2dF(1000, 0),
                       /*wait_before_processing=*/false);
    ASSERT_TRUE(FlingInProgress());

    if (NeedsBeginFrameForFlingProgress())
      ProgressFling(NowTicks());
  }

  // Stop the fling. This simulates hitting a scroll extent.
  {
    ASSERT_EQ(fling_controller_->CurrentFlingVelocity().x(), 1000);
    fling_controller_->StopFling();
  }

  // Now perform a second fling (e.g. after an infinite scroller loads more
  // content). Ensure it isn't boosted since the previous fling was
  // interrupted.
  {
    AdvanceTime(time_to_advance_ms);
    SimulateScrollBegin(blink::WebGestureDevice::kTouchscreen,
                        gfx::Vector2dF(10, 0));
    SimulateScrollUpdate(blink::WebGestureDevice::kTouchscreen,
                         gfx::Vector2dF(10, 0));
    SimulateFlingStart(blink::WebGestureDevice::kTouchscreen,
                       gfx::Vector2dF(1000, 0),
                       /*wait_before_processing=*/false);

    if (NeedsBeginFrameForFlingProgress())
      ProgressFling(NowTicks());

    EXPECT_EQ(fling_controller_->CurrentFlingVelocity().x(), 1000)
        << "Fling was boosted but should not have been.";
  }
}

TEST_P(FlingControllerTest, ControllerHandlesTouchscreenGestureFling) {
  SimulateFlingStart(blink::WebGestureDevice::kTouchscreen,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());

  // The fling progress will generate and send GSU events with inertial state.
  AdvanceTime();
  ProgressFling(NowTicks());
  ASSERT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            last_sent_gesture_.GetType());
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kMomentum,
            last_sent_gesture_.data.scroll_update.inertial_phase);
  EXPECT_GT(last_sent_gesture_.data.scroll_update.delta_x, 0.f);

  // Now cancel the fling.
  SimulateFlingCancel(blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(FlingInProgress());

  // Cancellation should send a GSE.
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollEnd,
            last_sent_gesture_.GetType());
}

TEST_P(FlingControllerTest, ControllerSendsWheelEndWhenTouchpadFlingIsOver) {
  SimulateFlingStart(blink::WebGestureDevice::kTouchpad,
                     gfx::Vector2dF(100, 0));
  EXPECT_TRUE(FlingInProgress());
  // Processing GFS will send the first fling progress event if the time delta
  // between the timestamp of the GFS and the time that ProcessGestureFlingStart
  // is called is large enough.
  bool process_GFS_sent_first_event = first_wheel_event_sent_;

  AdvanceTime();
  ProgressFling(NowTicks());
  if (!process_GFS_sent_first_event) {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, last_sent_wheel_.momentum_phase);
  } else {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged,
              last_sent_wheel_.momentum_phase);
  }
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  AdvanceTime();
  ProgressFling(NowTicks());
  while (FlingInProgress()) {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged,
              last_sent_wheel_.momentum_phase);
    EXPECT_GT(last_sent_wheel_.delta_x, 0.f);
    AdvanceTime();
    ProgressFling(NowTicks());
  }

  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, last_sent_wheel_.momentum_phase);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_x);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_y);
}

TEST_P(FlingControllerTest, ControllerSendsGSEWhenTouchscreenFlingIsOver) {
  SimulateFlingStart(blink::WebGestureDevice::kTouchscreen,
                     gfx::Vector2dF(100, 0));
  EXPECT_TRUE(FlingInProgress());

  AdvanceTime();
  ProgressFling(NowTicks());
  while (FlingInProgress()) {
    ASSERT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
              last_sent_gesture_.GetType());
    EXPECT_EQ(WebGestureEvent::InertialPhaseState::kMomentum,
              last_sent_gesture_.data.scroll_update.inertial_phase);
    EXPECT_GT(last_sent_gesture_.data.scroll_update.delta_x, 0.f);
    AdvanceTime();
    ProgressFling(NowTicks());
  }

  EXPECT_EQ(WebInputEvent::Type::kGestureScrollEnd,
            last_sent_gesture_.GetType());
}

TEST_P(FlingControllerTest, EarlyTouchpadFlingCancelationOnFlingStop) {
  SimulateFlingStart(blink::WebGestureDevice::kTouchpad,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  // Processing GFS will send the first fling progress event if the time delta
  // between the timestamp of the GFS and the time that ProcessGestureFlingStart
  // is called is large enough.
  bool process_GFS_sent_first_event = first_wheel_event_sent_;

  AdvanceTime();
  ProgressFling(NowTicks());
  if (!process_GFS_sent_first_event) {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, last_sent_wheel_.momentum_phase);
  } else {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged,
              last_sent_wheel_.momentum_phase);
  }
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  fling_controller_->StopFling();
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, last_sent_wheel_.momentum_phase);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_x);
  EXPECT_EQ(0.f, last_sent_wheel_.delta_y);
}

TEST_P(FlingControllerTest, EarlyTouchscreenFlingCancelationOnFlingStop) {
  SimulateFlingStart(blink::WebGestureDevice::kTouchscreen,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());

  // progress fling must send GSU events.
  AdvanceTime();
  ProgressFling(NowTicks());
  ASSERT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            last_sent_gesture_.GetType());
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kMomentum,
            last_sent_gesture_.data.scroll_update.inertial_phase);
  EXPECT_GT(last_sent_gesture_.data.scroll_update.delta_x, 0.f);

  fling_controller_->StopFling();
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollEnd,
            last_sent_gesture_.GetType());
}

TEST_P(FlingControllerTest, GestureFlingCancelOutsideFling) {
  // FlingCancel without a FlingStart doesn't cause issues, doesn't send any
  // events.
  {
    int current_sent_scroll_gesture_count = sent_scroll_gesture_count_;
    SimulateFlingCancel(blink::WebGestureDevice::kTouchscreen);
    EXPECT_FALSE(FlingInProgress());
    EXPECT_EQ(current_sent_scroll_gesture_count, sent_scroll_gesture_count_);
  }

  // Do a fling and cancel it. Make sure another cancel is also a no-op.
  {
    SimulateFlingStart(blink::WebGestureDevice::kTouchscreen,
                       gfx::Vector2dF(1000, 0));
    AdvanceTime();
    ProgressFling(NowTicks());
    SimulateFlingCancel(blink::WebGestureDevice::kTouchscreen);
    int current_sent_scroll_gesture_count = sent_scroll_gesture_count_;
    SimulateFlingCancel(blink::WebGestureDevice::kTouchscreen);
    EXPECT_EQ(current_sent_scroll_gesture_count, sent_scroll_gesture_count_);
  }
}

TEST_P(FlingControllerTest, GestureFlingNotCancelledBySmallTimeDelta) {
  SimulateFlingStart(blink::WebGestureDevice::kTouchscreen,
                     gfx::Vector2dF(1000, 0), false);
  EXPECT_TRUE(FlingInProgress());
  int current_sent_scroll_gesture_count = sent_scroll_gesture_count_;

  // If we the first progress tick happens too close to the fling_start time,
  // the controller won't send any GSU events, but the fling is still active.
  ProgressFling(NowTicks());
  EXPECT_EQ(current_sent_scroll_gesture_count, sent_scroll_gesture_count_);
  EXPECT_TRUE(FlingInProgress());

  // The rest of the progress flings must advance the fling normally.
  AdvanceTime();
  ProgressFling(NowTicks());
  EXPECT_EQ(blink::WebGestureDevice::kTouchscreen,
            last_sent_gesture_.SourceDevice());
  ASSERT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            last_sent_gesture_.GetType());
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kMomentum,
            last_sent_gesture_.data.scroll_update.inertial_phase);
  EXPECT_GT(last_sent_gesture_.data.scroll_update.delta_x, 0.f);
}

TEST_P(FlingControllerTest, GestureFlingWithNegativeTimeDelta) {
  base::TimeTicks initial_time = NowTicks();
  AdvanceTime();
  SimulateFlingStart(blink::WebGestureDevice::kTouchscreen,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  int current_sent_scroll_gesture_count = sent_scroll_gesture_count_;

  // If we get a negative time delta, that is, the Progress tick time happens
  // before the fling's start time then we should *not* try progressing the
  // fling.
  ProgressFling(initial_time);
  EXPECT_EQ(current_sent_scroll_gesture_count, sent_scroll_gesture_count_);

  // The rest of the progress flings must advance the fling normally.
  AdvanceTime();
  ProgressFling(NowTicks());
  EXPECT_EQ(blink::WebGestureDevice::kTouchscreen,
            last_sent_gesture_.SourceDevice());
  ASSERT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            last_sent_gesture_.GetType());
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kMomentum,
            last_sent_gesture_.data.scroll_update.inertial_phase);
  EXPECT_GT(last_sent_gesture_.data.scroll_update.delta_x, 0.f);
}

// Regression test for https://crbug.com/924279
TEST_P(FlingControllerTest, TouchpadFlingWithOldEvent) {
  // Only the code path that uses compositor animation observers is affected.
  if (NeedsBeginFrameForFlingProgress())
    return;

  // Create a fling start event.
  base::TimeTicks event_time = NowTicks();
  WebGestureEvent fling_start(WebInputEvent::Type::kGestureFlingStart, 0,
                              event_time, blink::WebGestureDevice::kTouchpad);
  fling_start.data.fling_start.velocity_x = 0.f;
  fling_start.data.fling_start.velocity_y = -1000.f;
  GestureEventWithLatencyInfo fling_start_with_latency(fling_start);

  // Move time forward. Assume a frame occurs here.
  AdvanceTime(1.f);
  base::TimeTicks last_frame_time = NowTicks();

  // Start the fling animation later, as if there was a delay in event dispatch.
  AdvanceTime(1.f);
  fling_controller_->ProcessGestureFlingStart(fling_start_with_latency);
  EXPECT_TRUE(FlingInProgress());

  // Initial scroll was sent.
  EXPECT_EQ(1, wheel_event_count_);
  wheel_event_count_ = 0;

  // Move time forward a little.
  AdvanceTime(1.f);

  // Simulate the compositor animation observer calling ProgressFling with the
  // last frame time. That frame time is after the event time, but before the
  // animation start time.
  ProgressFling(last_frame_time);

  // No scrolls were sent.
  EXPECT_EQ(0, wheel_event_count_);

  // Additional ProgressFling calls generate scroll events as normal.
  AdvanceTime();
  ProgressFling(NowTicks());
  EXPECT_GT(wheel_event_count_, 0);
}

TEST_P(FlingControllerTest, ControllerBoostsTouchpadFling) {
  SimulateFlingStart(blink::WebGestureDevice::kTouchpad,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  // Processing GFS will send the first fling progress event if the time delta
  // between the timestamp of the GFS and the time that ProcessGestureFlingStart
  // is called is large enough.
  bool process_GFS_sent_first_event = first_wheel_event_sent_;

  AdvanceTime();
  ProgressFling(NowTicks());
  if (!process_GFS_sent_first_event) {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, last_sent_wheel_.momentum_phase);
  } else {
    EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged,
              last_sent_wheel_.momentum_phase);
  }
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  // The rest of the wheel events must have momentum_phase == KPhaseChanged.
  AdvanceTime();
  ProgressFling(NowTicks());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged, last_sent_wheel_.momentum_phase);
  EXPECT_GT(last_sent_wheel_.delta_x, 0.f);

  // Now cancel the fling.
  SimulateFlingCancel(blink::WebGestureDevice::kTouchpad);
  EXPECT_FALSE(FlingInProgress());

  // The second GFS will boost the current active fling.
  SimulateFlingStart(blink::WebGestureDevice::kTouchpad,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
}

TEST_P(FlingControllerTest, ControllerBoostsTouchscreenFling) {
  SimulateFlingStart(blink::WebGestureDevice::kTouchscreen,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  // Fling progress must send GSU events.
  AdvanceTime();
  ProgressFling(NowTicks());
  ASSERT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            last_sent_gesture_.GetType());
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kMomentum,
            last_sent_gesture_.data.scroll_update.inertial_phase);
  EXPECT_GT(last_sent_gesture_.data.scroll_update.delta_x, 0.f);

  // Now cancel the fling.
  SimulateFlingCancel(blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(FlingInProgress());

  // The second GFS can be boosted so it should boost the just deactivated
  // fling.
  SimulateFlingStart(blink::WebGestureDevice::kTouchscreen,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  EXPECT_GT(fling_controller_->CurrentFlingVelocity().x(), 1000);
}

// Ensure that once a fling finishes, the next fling doesn't get boosted.
TEST_P(FlingControllerTest, ControllerDoesntBoostFinishedFling) {
  SimulateFlingStart(blink::WebGestureDevice::kTouchscreen,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());
  AdvanceTime();
  ProgressFling(NowTicks());

  // Fast forward so that the fling ends.
  double time_to_advance_ms = 1000.0;
  AdvanceTime(time_to_advance_ms);
  ProgressFling(NowTicks());
  ASSERT_EQ(WebInputEvent::Type::kGestureScrollEnd,
            last_sent_gesture_.GetType())
      << "Unexpected Last Sent Gesture: "
      << WebInputEvent::GetName(last_sent_gesture_.GetType());
  EXPECT_EQ(fling_controller_->CurrentFlingVelocity().x(), 0);
  EXPECT_FALSE(FlingInProgress());

  // Now send a new fling that would have been boosted had it occurred during
  // the previous fling. Ensure it isn't boosted.
  AdvanceTime();
  SimulateFlingCancel(blink::WebGestureDevice::kTouchscreen);
  SimulateFlingStart(blink::WebGestureDevice::kTouchscreen,
                     gfx::Vector2dF(1000, 0), /*wait_before_processing=*/false);
  EXPECT_TRUE(FlingInProgress());
  EXPECT_EQ(fling_controller_->CurrentFlingVelocity().x(), 1000);
}

TEST_P(FlingControllerTest, ControllerNotifiesTheClientAfterFlingStart) {
  SimulateFlingStart(blink::WebGestureDevice::kTouchscreen,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());

  // Now cancel the fling.
  SimulateFlingCancel(blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_TRUE(notified_client_after_fling_stop_);
}

TEST_P(FlingControllerTest, MiddleClickAutoScrollFling) {
  SimulateFlingStart(blink::WebGestureDevice::kSyntheticAutoscroll,
                     gfx::Vector2dF(1000, 0));
  EXPECT_TRUE(FlingInProgress());

  AdvanceTime();
  ProgressFling(NowTicks());
  ASSERT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            last_sent_gesture_.GetType());
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kMomentum,
            last_sent_gesture_.data.scroll_update.inertial_phase);
  EXPECT_GT(last_sent_gesture_.data.scroll_update.delta_x, 0.f);

  // Now send a new fling with different velocity and without sending a fling
  // cancel event, the new fling should always replace the old one even when
  // they are in the same direction.
  SimulateFlingStart(blink::WebGestureDevice::kSyntheticAutoscroll,
                     gfx::Vector2dF(2000, 0));
  EXPECT_TRUE(FlingInProgress());
  EXPECT_EQ(fling_controller_->CurrentFlingVelocity().x(), 2000);

  // Now cancel the fling.
  SimulateFlingCancel(blink::WebGestureDevice::kSyntheticAutoscroll);
  EXPECT_FALSE(FlingInProgress());
}

// Ensure that the fling controller does not start a fling if the last touchpad
// wheel event was consumed.
TEST_P(FlingControllerTest, NoFlingStartAfterWheelEventConsumed) {
  // First ensure that a fling can start after a not consumed wheel event.
  fling_controller_->OnWheelEventAck(
      MouseWheelEventWithLatencyInfo(),
      blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kNotConsumed);

  SimulateFlingStart(blink::WebGestureDevice::kTouchpad,
                     gfx::Vector2dF(1000, 0));
  ASSERT_TRUE(FlingInProgress());

  // Cancel the first fling.
  SimulateFlingCancel(blink::WebGestureDevice::kTouchpad);
  EXPECT_FALSE(FlingInProgress());

  // Now test that a consumed touchpad wheel event results in no fling.
  fling_controller_->OnWheelEventAck(
      MouseWheelEventWithLatencyInfo(),
      blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kConsumed);

  SimulateFlingStart(blink::WebGestureDevice::kTouchpad,
                     gfx::Vector2dF(1000, 0));
  EXPECT_FALSE(FlingInProgress());
}

class FlingControllerWithPhysicsBasedFlingTest : public FlingControllerTest {
 public:
  // testing::Test
  FlingControllerWithPhysicsBasedFlingTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kExperimentalFlingAnimation);
  }

  FlingControllerWithPhysicsBasedFlingTest(
      const FlingControllerWithPhysicsBasedFlingTest&) = delete;
  FlingControllerWithPhysicsBasedFlingTest& operator=(
      const FlingControllerWithPhysicsBasedFlingTest&) = delete;

  ~FlingControllerWithPhysicsBasedFlingTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         FlingControllerWithPhysicsBasedFlingTest,
                         testing::Bool());

// Ensure the bounding distance for boosted physics based flings is increased
// by a factor of the boost_multiplier and default multiplier
TEST_P(FlingControllerWithPhysicsBasedFlingTest,
       ControllerBoostsTouchscreenFling) {
  // We use a velocity of 4500 in this test because it yields a scroll delta
  // that is greater than viewport * boost_multiplier * kDefaultBoundsMultiplier

  // Android, Chromecast and iOS use Mobile fling curve so they are ignored
  // for this test
  bool use_mobile_fling_curve = false;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CASTOS) || BUILDFLAG(IS_IOS)
  use_mobile_fling_curve = true;
#endif
  if (use_mobile_fling_curve)
    return;

  SimulateFlingStart(blink::WebGestureDevice::kTouchscreen,
                     gfx::Vector2dF(4500, 0));
  EXPECT_TRUE(FlingInProgress());
  // Fling progress must send GSU events.
  AdvanceTime();
  ProgressFling(NowTicks());
  ASSERT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            last_sent_gesture_.GetType());
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kMomentum,
            last_sent_gesture_.data.scroll_update.inertial_phase);
  EXPECT_GT(last_sent_gesture_.data.scroll_update.delta_x, 0.f);

  // Now cancel the fling.
  SimulateFlingCancel(blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(FlingInProgress());

  // The second GFS can be boosted so it should boost the just deactivated
  // fling. To test that the correct bounds scale is used, the scroll delta
  // is accumulated after each frame.
  SimulateFlingStart(blink::WebGestureDevice::kTouchscreen,
                     gfx::Vector2dF(4500, 0));
  EXPECT_TRUE(FlingInProgress());
  if (NeedsBeginFrameForFlingProgress())
    ProgressFling(NowTicks());

  float total_scroll_delta = CompleteFlingAndAccumulateScrollDelta();

  // We expect the scroll delta to be the viewport * [boost_multiplier = 2] *
  // multiplier
  float expected_delta =
      2 * PhysicsBasedFlingCurve::default_bounds_multiplier_for_testing() *
      GetRootWidgetViewportSize().width();
  EXPECT_EQ(ceilf(total_scroll_delta), roundf(expected_delta));
}

// Ensure that once a fling finishes, the next fling has a boost_multiplier of 1
TEST_P(FlingControllerWithPhysicsBasedFlingTest,
       ControllerDoesntBoostFinishedFling) {
  // Android, Chromecast and iOS use Mobile fling curve so they are ignored
  // for this test
  bool use_mobile_fling_curve = false;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CASTOS) || BUILDFLAG(IS_IOS)
  use_mobile_fling_curve = true;
#endif
  if (use_mobile_fling_curve)
    return;
  SimulateFlingStart(blink::WebGestureDevice::kTouchscreen,
                     gfx::Vector2dF(1000, 0), /*wait_before_processing=*/true);
  EXPECT_TRUE(FlingInProgress());
  AdvanceTime();
  ProgressFling(NowTicks());

  // Fast forward so that the fling ends.
  double time_to_advance_ms = 1000.0;
  AdvanceTime(time_to_advance_ms);
  ProgressFling(NowTicks());
  ASSERT_EQ(WebInputEvent::Type::kGestureScrollEnd,
            last_sent_gesture_.GetType())
      << "Unexpected Last Sent Gesture: "
      << WebInputEvent::GetName(last_sent_gesture_.GetType());
  EXPECT_EQ(fling_controller_->CurrentFlingVelocity().x(), 0);
  EXPECT_FALSE(FlingInProgress());

  // Now send a new fling, ensure boost_multiplier is 1
  AdvanceTime();
  SimulateFlingCancel(blink::WebGestureDevice::kTouchscreen);

  SimulateFlingStart(blink::WebGestureDevice::kTouchscreen,
                     gfx::Vector2dF(10000, 0));
  EXPECT_TRUE(FlingInProgress());
  if (NeedsBeginFrameForFlingProgress())
    ProgressFling(NowTicks());

  float total_scroll_delta = CompleteFlingAndAccumulateScrollDelta();

  // We expect the scroll delta to be the viewport * [boost_multiplier = 1] *
  // multiplier
  float expected_delta =
      PhysicsBasedFlingCurve::default_bounds_multiplier_for_testing() *
      GetRootWidgetViewportSize().width();
  EXPECT_EQ(ceilf(total_scroll_delta), roundf(expected_delta));
}

}  // namespace input
