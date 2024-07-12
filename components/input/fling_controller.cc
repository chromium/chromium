// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/fling_controller.h"

#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/gestures/blink/web_gesture_curve_impl.h"

using blink::WebInputEvent;
using blink::WebGestureEvent;

namespace {
constexpr base::TimeDelta kFrameDelta = base::Seconds(1.0 / 60.0);

// Maximum time between a fling event's timestamp and the first |Progress| call
// for the fling curve to use the fling timestamp as the initial animation time.
// Two frames allows a minor delay between event creation and the first
// progress.
constexpr base::TimeDelta kMaxMicrosecondsFromFlingTimestampToFirstProgress =
    base::Seconds(2.0 / 60.0);

// Since the progress fling is called in ProcessGestureFlingStart right after
// processing the GFS, it is possible to have a very small delta for the first
// event. Don't send an event with deltas smaller than the
// |kMinInertialScrollDelta| since the renderer ignores it and the fling gets
// cancelled in RenderWidgetHostViewAndroid::GestureEventAck due to an inertial
// GSU with ack ignored.
const float kMinInertialScrollDelta = 0.1f;

const char* kFlingTraceName = "FlingController::HandlingGestureFling";

}  // namespace

namespace input {

FlingController::Config::Config() {}

FlingController::FlingController(
    FlingControllerEventSenderClient* event_sender_client,
    FlingControllerSchedulerClient* scheduler_client,
    const Config& config)
    : event_sender_client_(event_sender_client),
      scheduler_client_(scheduler_client),
      touchpad_tap_suppression_controller_(
          config.touchpad_tap_suppression_config),
      touchscreen_tap_suppression_controller_(
          config.touchscreen_tap_suppression_config),
      clock_(base::DefaultTickClock::GetInstance()) {
  DCHECK(event_sender_client);
  DCHECK(scheduler_client);
}

FlingController::~FlingController() = default;

bool FlingController::ObserveAndFilterForTapSuppression(
    const GestureEventWithLatencyInfo& gesture_event) {
  switch (gesture_event.event.GetType()) {
    case WebInputEvent::Type::kGestureFlingCancel:
      // The controllers' state is affected by the cancel event and assumes
      // it's actually stopping an ongoing fling.
      DCHECK(fling_curve_);
      if (gesture_event.event.SourceDevice() ==
          blink::WebGestureDevice::kTouchscreen) {
        touchscreen_tap_suppression_controller_
            .GestureFlingCancelStoppedFling();
      } else if (gesture_event.event.SourceDevice() ==
                 blink::WebGestureDevice::kTouchpad) {
        touchpad_tap_suppression_controller_.GestureFlingCancelStoppedFling();
      }
      return false;
    case WebInputEvent::Type::kGestureTapDown:
    case WebInputEvent::Type::kGestureShowPress:
    case WebInputEvent::Type::kGestureTapUnconfirmed:
    case WebInputEvent::Type::kGestureTapCancel:
    case WebInputEvent::Type::kGestureTap:
    case WebInputEvent::Type::kGestureDoubleTap:
    case WebInputEvent::Type::kGestureLongPress:
    case WebInputEvent::Type::kGestureLongTap:
    case WebInputEvent::Type::kGestureTwoFingerTap:
      if (gesture_event.event.SourceDevice() ==
          blink::WebGestureDevice::kTouchscreen) {
        return touchscreen_tap_suppression_controller_.FilterTapEvent(
            gesture_event);
      }
      return false;
    default:
      return false;
  }
}

bool FlingController::ObserveAndMaybeConsumeGestureEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  TRACE_EVENT0("input", "FlingController::ObserveAndMaybeConsumeGestureEvent");
  // FlingCancel events arrive when a finger is touched down regardless of
  // whether there is an ongoing fling. These can affect state so if there's no
  // on-going fling we should just discard these without letting the rest of
  // the fling system see it.
  if (gesture_event.event.GetType() ==
          WebInputEvent::Type::kGestureFlingCancel &&
      !fling_curve_) {
    TRACE_EVENT_INSTANT0("input", "NoActiveFling", TRACE_EVENT_SCOPE_THREAD);
    return true;
  }

  if (ObserveAndFilterForTapSuppression(gesture_event)) {
    TRACE_EVENT_INSTANT0("input", "FilterTapSuppression",
                         TRACE_EVENT_SCOPE_THREAD);
    return true;
  }
  if (gesture_event.event.GetType() ==
      WebInputEvent::Type::kGestureScrollUpdate) {
    last_seen_scroll_update_ = gesture_event.event.TimeStamp();
  } else if (gesture_event.event.GetType() ==
                 WebInputEvent::Type::kGestureScrollEnd ||
             gesture_event.event.GetType() ==
                 WebInputEvent::Type::kGestureScrollBegin) {
    // TODO(bokan): We reset this on Begin as well as End since there appear to
    // be cases where we see an invalid event sequence:
    // https://crbug.com/928569.
    last_seen_scroll_update_ = base::TimeTicks();
  }

  fling_booster_.ObserveGestureEvent(gesture_event.event);

  // fling_controller_ is in charge of handling GFS events and the events are
  // not sent to the renderer, the controller processes the fling and generates
  // fling progress events (wheel events for touchpad and GSU events for
  // touchscreen and autoscroll) which are handled normally.
  if (gesture_event.event.GetType() ==
      WebInputEvent::Type::kGestureFlingStart) {
    ProcessGestureFlingStart(gesture_event);
    return true;
  }

  // If the GestureFlingStart event is processed by the fling_controller_, the
  // GestureFlingCancel event should be the same.
  if (gesture_event.event.GetType() ==
      WebInputEvent::Type::kGestureFlingCancel) {
    ProcessGestureFlingCancel(gesture_event);
    return true;
  }

  return false;
}

void FlingController::ProcessGestureFlingStart(
    const GestureEventWithLatencyInfo& gesture_event) {
  // Don't start a touchpad gesture fling if the previous scroll events were
  // consumed.
  if (gesture_event.event.SourceDevice() ==
          blink::WebGestureDevice::kTouchpad &&
      last_wheel_event_consumed_) {
    return;
  }

  if (!UpdateCurrentFlingState(gesture_event.event))
    return;

  TRACE_EVENT_ASYNC_BEGIN2("input", kFlingTraceName, this, "vx",
                           current_fling_parameters_.velocity.x(), "vy",
                           current_fling_parameters_.velocity.y());

  last_progress_time_ = base::TimeTicks();

  // Wait for BeginFrame to call ProgressFling when
  // SetNeedsBeginFrameForFlingProgress is used to progress flings instead of
  // compositor animation observer (happens on Android WebView).
  if (scheduler_client_->NeedsBeginFrameForFlingProgress())
    ScheduleFlingProgress();
  else
    ProgressFling(clock_->NowTicks());
}

void FlingController::ScheduleFlingProgress() {
  scheduler_client_->ScheduleFlingProgress(weak_ptr_factory_.GetWeakPtr());
}

void FlingController::ProcessGestureFlingCancel(
    const GestureEventWithLatencyInfo& gesture_event) {
  DCHECK(fling_curve_);

  // Note: We don't want to reset the fling booster here because a FlingCancel
  // will be received when the user puts their finger down for a potential
  // boost. FlingBooster will process the event stream after the current fling
  // is ended and decide whether or not to boost any subsequent FlingStart.
  EndCurrentFling(gesture_event.event.TimeStamp());
}

void FlingController::ProgressFling(base::TimeTicks current_time) {
  if (!fling_curve_)
    return;

  TRACE_EVENT_ASYNC_STEP_INTO0("input", kFlingTraceName, this, "ProgressFling");

  if (!first_fling_update_sent()) {
    // Guard against invalid as there are no guarantees fling event and progress
    // timestamps are compatible.
    if (current_fling_parameters_.start_time.is_null()) {
      current_fling_parameters_.start_time = current_time;
      ScheduleFlingProgress();
      return;
    }

    // If the first time that progressFling is called is more than two frames
    // later than the fling start time, delay the fling start time to one frame
    // prior to the current time. This makes sure that at least one progress
    // event is sent while the fling is active even when the fling duration is
    // short (small velocity) and the time delta between its timestamp and its
    // processing time is big (e.g. When a GFS gets bubbled from an oopif).
    if (current_time >= current_fling_parameters_.start_time +
                            kMaxMicrosecondsFromFlingTimestampToFirstProgress) {
      current_fling_parameters_.start_time = current_time - kFrameDelta;
    }
  }

  // ProgressFling is called inside FlingScheduler::OnAnimationStep. Sometimes
  // the first OnAnimationStep call has the time of the last frame before
  // AddAnimationObserver call rather than time of the first frame after
  // AddAnimationObserver call. Do not advance the fling when current_time is
  // less than last fling progress time or less than the GFS event timestamp.
  if (current_time < last_progress_time_ ||
      current_time <= current_fling_parameters_.start_time) {
    ScheduleFlingProgress();
    return;
  }

  gfx::Vector2dF delta_to_scroll;
  bool fling_is_active = fling_curve_->Advance(
      (current_time - current_fling_parameters_.start_time).InSecondsF(),
      current_fling_parameters_.velocity, delta_to_scroll);

  if (!fling_is_active && current_fling_parameters_.source_device !=
                              blink::WebGestureDevice::kSyntheticAutoscroll) {
    fling_booster_.Reset();
    EndCurrentFling(current_time);
    return;
  }

  if (std::abs(delta_to_scroll.x()) > kMinInertialScrollDelta ||
      std::abs(delta_to_scroll.y()) > kMinInertialScrollDelta) {
    GenerateAndSendFlingProgressEvents(current_time, delta_to_scroll);
    last_progress_time_ = current_time;
  }

  // As long as the fling curve is active, the fling progress must get
  // scheduled even when the last delta to scroll was zero.
  if (fling_curve_) {
    ScheduleFlingProgress();
  }
}

void FlingController::StopFling() {
  fling_booster_.Reset();
  if (fling_curve_)
    EndCurrentFling(clock_->NowTicks());
}

void FlingController::GenerateAndSendWheelEvents(
    base::TimeTicks current_time,
    const gfx::Vector2dF& delta,
    blink::WebMouseWheelEvent::Phase phase) {
  MouseWheelEventWithLatencyInfo synthetic_wheel(
      WebInputEvent::Type::kMouseWheel, current_fling_parameters_.modifiers,
      current_time, ui::LatencyInfo());
  synthetic_wheel.event.delta_units =
      ui::ScrollGranularity::kScrollByPrecisePixel;
  synthetic_wheel.event.delta_x = delta.x();
  synthetic_wheel.event.delta_y = delta.y();
  synthetic_wheel.event.momentum_phase = phase;
  synthetic_wheel.event.has_synthetic_phase = true;
  synthetic_wheel.event.SetPositionInWidget(current_fling_parameters_.point);
  synthetic_wheel.event.SetPositionInScreen(
      current_fling_parameters_.global_point);
  // Send wheel events nonblocking.
  synthetic_wheel.event.dispatch_type =
      WebInputEvent::DispatchType::kEventNonBlocking;

  event_sender_client_->SendGeneratedWheelEvent(synthetic_wheel);
}

void FlingController::GenerateAndSendGestureScrollEvents(
    base::TimeTicks current_time,
    WebInputEvent::Type type,
    const gfx::Vector2dF& delta /* = gfx::Vector2dF() */) {
  GestureEventWithLatencyInfo synthetic_gesture(
      type, current_fling_parameters_.modifiers, current_time,
      ui::LatencyInfo());
  synthetic_gesture.event.SetPositionInWidget(current_fling_parameters_.point);
  synthetic_gesture.event.SetPositionInScreen(
      current_fling_parameters_.global_point);
  synthetic_gesture.event.primary_pointer_type =
      blink::WebPointerProperties::PointerType::kTouch;
  synthetic_gesture.event.SetSourceDevice(
      current_fling_parameters_.source_device);
  if (type == WebInputEvent::Type::kGestureScrollUpdate) {
    synthetic_gesture.event.data.scroll_update.delta_x = delta.x();
    synthetic_gesture.event.data.scroll_update.delta_y = delta.y();
    synthetic_gesture.event.data.scroll_update.inertial_phase =
        WebGestureEvent::InertialPhaseState::kMomentum;
  } else {
    DCHECK_EQ(WebInputEvent::Type::kGestureScrollEnd, type);
    synthetic_gesture.event.data.scroll_end.inertial_phase =
        WebGestureEvent::InertialPhaseState::kMomentum;
    synthetic_gesture.event.data.scroll_end.generated_by_fling_controller =
        true;
  }
  event_sender_client_->SendGeneratedGestureScrollEvents(synthetic_gesture);
}

void FlingController::GenerateAndSendFlingProgressEvents(
    base::TimeTicks current_time,
    const gfx::Vector2dF& delta) {
  switch (current_fling_parameters_.source_device) {
    case blink::WebGestureDevice::kTouchpad: {
      blink::WebMouseWheelEvent::Phase phase =
          first_fling_update_sent() ? blink::WebMouseWheelEvent::kPhaseChanged
                                    : blink::WebMouseWheelEvent::kPhaseBegan;
      GenerateAndSendWheelEvents(current_time, delta, phase);
      break;
    }
    case blink::WebGestureDevice::kTouchscreen:
    case blink::WebGestureDevice::kSyntheticAutoscroll:
      GenerateAndSendGestureScrollEvents(
          current_time, WebInputEvent::Type::kGestureScrollUpdate, delta);
      break;
    case blink::WebGestureDevice::kUninitialized:
    case blink::WebGestureDevice::kScrollbar:
      NOTREACHED_IN_MIGRATION()
          << "Fling controller doesn't handle flings with source device:"
          << static_cast<int>(current_fling_parameters_.source_device);
  }
  fling_booster_.ObserveProgressFling(current_fling_parameters_.velocity);
}

void FlingController::GenerateAndSendFlingEndEvents(
    base::TimeTicks current_time) {
  switch (current_fling_parameters_.source_device) {
    case blink::WebGestureDevice::kTouchpad:
      GenerateAndSendWheelEvents(current_time, gfx::Vector2d(),
                                 blink::WebMouseWheelEvent::kPhaseEnded);
      break;
    case blink::WebGestureDevice::kTouchscreen:
    case blink::WebGestureDevice::kSyntheticAutoscroll:
      GenerateAndSendGestureScrollEvents(
          current_time, WebInputEvent::Type::kGestureScrollEnd);
      break;
    case blink::WebGestureDevice::kUninitialized:
    case blink::WebGestureDevice::kScrollbar:
      NOTREACHED_IN_MIGRATION()
          << "Fling controller doesn't handle flings with source device:"
          << static_cast<int>(current_fling_parameters_.source_device);
  }
}

void FlingController::EndCurrentFling(base::TimeTicks current_time) {
  last_progress_time_ = base::TimeTicks();

  GenerateAndSendFlingEndEvents(current_time);
  current_fling_parameters_ = ActiveFlingParameters();

  if (fling_curve_) {
    scheduler_client_->DidStopFlingingOnBrowser(weak_ptr_factory_.GetWeakPtr());
    TRACE_EVENT_ASYNC_END0("input", kFlingTraceName, this);
  }

  fling_curve_.reset();
}

bool FlingController::UpdateCurrentFlingState(
    const WebGestureEvent& fling_start_event) {
  DCHECK_EQ(WebInputEvent::Type::kGestureFlingStart,
            fling_start_event.GetType());

  const gfx::Vector2dF velocity =
      fling_booster_.GetVelocityForFlingStart(fling_start_event);

  current_fling_parameters_.velocity = velocity;
  current_fling_parameters_.point = fling_start_event.PositionInWidget();
  current_fling_parameters_.global_point = fling_start_event.PositionInScreen();
  current_fling_parameters_.modifiers = fling_start_event.GetModifiers();
  current_fling_parameters_.source_device = fling_start_event.SourceDevice();

  if (fling_start_event.SourceDevice() ==
          blink::WebGestureDevice::kSyntheticAutoscroll ||
      last_seen_scroll_update_.is_null()) {
    current_fling_parameters_.start_time = fling_start_event.TimeStamp();
  } else {
    // To maintain a smooth, continuous transition from a drag scroll to a fling
    // scroll, the animation should begin at the time of the last update.
    current_fling_parameters_.start_time = last_seen_scroll_update_;
  }

  if (velocity.IsZero() && fling_start_event.SourceDevice() !=
                               blink::WebGestureDevice::kSyntheticAutoscroll) {
    fling_booster_.Reset();
    EndCurrentFling(fling_start_event.TimeStamp());
    return false;
  }

  gfx::Size root_widget_viewport_size =
      event_sender_client_->GetRootWidgetViewportSize();
  // If the view is destroyed while FlingController is generating fling curve,
  // |GetRootWidgetViewportSize()| will return empty size. Reset the
  // state of fling_booster_ and return false.
  if (root_widget_viewport_size.IsEmpty()) {
    fling_booster_.Reset();
    EndCurrentFling(last_seen_scroll_update_);
    return false;
  }

  gfx::Vector2dF velocity_from_gfs(
      fling_start_event.data.fling_start.velocity_x,
      fling_start_event.data.fling_start.velocity_y);

  float max_velocity_from_gfs =
      std::max(fabs(velocity_from_gfs.x()), fabs(velocity_from_gfs.y()));
  float max_velocity = std::max(fabs(current_fling_parameters_.velocity.x()),
                                fabs(current_fling_parameters_.velocity.y()));

  // Scale the default bound multiplier to compute the maximum scroll distance a
  // fling can travel based on physics based fling curve.
  float boost_multiplier = max_velocity / max_velocity_from_gfs;

  fling_curve_ = ui::WebGestureCurveImpl::CreateFromDefaultPlatformCurve(
          current_fling_parameters_.source_device,
          current_fling_parameters_.velocity,
          gfx::Vector2dF() /*initial_offset*/, false /*on_main_thread*/,
          scheduler_client_->ShouldUseMobileFlingCurve(),
          scheduler_client_->GetPixelsPerInch(
              current_fling_parameters_.global_point),
          boost_multiplier, root_widget_viewport_size);
  return true;
}

gfx::Vector2dF FlingController::CurrentFlingVelocity() const {
  return current_fling_parameters_.velocity;
}

TouchpadTapSuppressionController*
FlingController::GetTouchpadTapSuppressionController() {
  return &touchpad_tap_suppression_controller_;
}

void FlingController::OnWheelEventAck(
    const MouseWheelEventWithLatencyInfo& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  last_wheel_event_consumed_ =
      (ack_result == blink::mojom::InputEventResultState::kConsumed);
}

}  // namespace input
