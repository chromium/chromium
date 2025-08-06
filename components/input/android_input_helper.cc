// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/android_input_helper.h"

#include "base/metrics/histogram_macros.h"
#include "components/input/events_helper.h"
#include "components/input/render_input_router.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/input/switches.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/event_utils.h"

namespace input {

AndroidInputHelper::~AndroidInputHelper() = default;

AndroidInputHelper::AndroidInputHelper(RenderWidgetHostViewInput* view,
                                       Delegate* delegate)
    : view_(*view), delegate_(*delegate) {}

void AndroidInputHelper::RouteOrForwardTouchEvent(
    blink::WebTouchEvent& web_event) {
  RenderInputRouter* rir = view_->GetViewRenderInputRouter();
  CHECK(rir);
  ui::LatencyInfo latency_info;
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
  if (ShouldRouteEvents()) {
    rir->delegate()->GetInputEventRouter()->RouteTouchEvent(&*view_, &web_event,
                                                            latency_info);
  } else {
    rir->ForwardTouchEventWithLatencyInfo(web_event, latency_info);
  }
}

void AndroidInputHelper::RouteOrForwardGestureEvent(
    const blink::WebGestureEvent& event) {
  RenderInputRouter* rir = view_->GetViewRenderInputRouter();
  CHECK(rir);

  ui::LatencyInfo latency_info;
  if (ShouldRouteEvents()) {
    blink::WebGestureEvent gesture_event(event);
    rir->delegate()->GetInputEventRouter()->RouteGestureEvent(
        &*view_, &gesture_event, latency_info);
  } else {
    rir->ForwardGestureEventWithLatencyInfo(event, latency_info);
  }
}

bool AndroidInputHelper::ShouldRouteEvents() const {
  CHECK(view_->GetViewRenderInputRouter());
  return view_->GetViewRenderInputRouter()->delegate() &&
         view_->GetViewRenderInputRouter()->delegate()->GetInputEventRouter();
}

void AndroidInputHelper::ResetGestureDetection() {
  ui::FilteredGestureProvider& gesture_provider =
      delegate_->GetGestureProvider();

  const ui::MotionEvent* current_down_event =
      gesture_provider.GetCurrentDownEvent();
  if (!current_down_event) {
    // A hard reset ensures prevention of any timer-based events that might fire
    // after a touch sequence has ended.
    gesture_provider.ResetDetection();
    return;
  }

  const ui::MotionEvent* last_event =
      gesture_provider.GetLastEventWithoutHistory();
  CHECK(last_event);

  std::unique_ptr<ui::MotionEvent> cancel_event;
  if (last_event->GetAction() == ui::MotionEvent::Action::POINTER_UP &&
      last_event->GetPointerCount() == 1) {
    // Fall back to using down for generating cancel, since we expect pointer
    // ups to generally have all the pointers in it.
    cancel_event = current_down_event->Cancel();
  } else {
    cancel_event = last_event->Cancel();
  }
  if (gesture_provider.OnTouchEvent(*cancel_event).succeeded) {
    blink::WebTouchEvent web_event = ui::CreateWebTouchEventFromMotionEvent(
        *cancel_event, false /* may_cause_scrolling */, false /* hovering */);
    RouteOrForwardTouchEvent(web_event);
  }
}

bool AndroidInputHelper::RequiresDoubleTapGestureEvents() const {
  return true;
}

void AndroidInputHelper::OnGestureEvent(const ui::GestureEventData& gesture) {
  if ((gesture.type() == ui::EventType::kGesturePinchBegin ||
       gesture.type() == ui::EventType::kGesturePinchUpdate ||
       gesture.type() == ui::EventType::kGesturePinchEnd) &&
      !switches::IsPinchToZoomEnabled()) {
    return;
  }

  blink::WebGestureEvent web_gesture =
      ui::CreateWebGestureEventFromGestureEventData(gesture);
  // TODO(jdduke): Remove this workaround after Android fixes UiAutomator to
  // stop providing shift meta values to synthetic MotionEvents. This prevents
  // unintended shift+click interpretation of all accessibility clicks.
  // See crbug.com/443247.
  if (web_gesture.GetType() == blink::WebInputEvent::Type::kGestureTap &&
      web_gesture.GetModifiers() == blink::WebInputEvent::kShiftKey) {
    web_gesture.SetModifiers(blink::WebInputEvent::kNoModifiers);
  }
  delegate_->SendGestureEvent(web_gesture);
}

void AndroidInputHelper::ProcessAckedTouchEvent(
    const input::TouchEventWithLatencyInfo& touch,
    blink::mojom::InputEventResultState ack_result) {
  const bool event_consumed =
      ack_result == blink::mojom::InputEventResultState::kConsumed;
  // |is_source_touch_event_set_non_blocking| defines a blocking behaviour of
  // the future inputs.
  const bool is_source_touch_event_set_non_blocking =
      input::InputEventResultStateIsSetBlocking(ack_result);
  // |was_touch_blocked| indicates whether the current event was dispatched
  // blocking to the Renderer.
  const bool was_touch_blocked =
      ui::WebInputEventTraits::ShouldBlockEventStream(touch.event);
  delegate_->GetGestureProvider().OnTouchEventAck(
      touch.event.unique_touch_event_id, event_consumed,
      is_source_touch_event_set_non_blocking,
      was_touch_blocked
          ? std::make_optional(touch.event.GetEventLatencyMetadata())
          : std::nullopt);
  if (touch.event.touch_start_or_first_touch_move && event_consumed &&
      view_->GetViewRenderInputRouter()->delegate() &&
      view_->GetViewRenderInputRouter()->delegate()->GetInputEventRouter()) {
    view_->GetViewRenderInputRouter()
        ->delegate()
        ->GetInputEventRouter()
        ->OnHandledTouchStartOrFirstTouchMove(
            touch.event.unique_touch_event_id);
  }
}

bool AndroidInputHelper::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    input::RenderWidgetHostViewInput* target_view,
    gfx::PointF* transformed_point) {
  if (target_view == &*view_) {
    *transformed_point = point;
    return true;
  }

  if (!view_->GetFrameSinkId().is_valid()) {
    return false;
  }

  // In TransformPointToLocalCoordSpace() there is a Point-to-Pixel
  // conversion, but it is not necessary here because the final target view
  // is responsible for converting before computing the final transform.
  return target_view->TransformPointToLocalCoordSpace(
      point, view_->GetFrameSinkId(), transformed_point);
}

void AndroidInputHelper::RecordToolTypeForActionDown(
    const ui::MotionEventAndroid& event) {
  ui::MotionEventAndroid::Action action = event.GetAction();
  if (action == ui::MotionEventAndroid::Action::DOWN ||
      action == ui::MotionEventAndroid::Action::POINTER_DOWN ||
      action == ui::MotionEventAndroid::Action::BUTTON_PRESS) {
    UMA_HISTOGRAM_ENUMERATION(
        "Event.AndroidActionDown.ToolType",
        static_cast<int>(event.GetToolType(0)),
        static_cast<int>(ui::MotionEventAndroid::ToolType::LAST) + 1);
  }
}

void AndroidInputHelper::ComputeEventLatencyOSTouchHistograms(
    const ui::MotionEvent& event,
    const base::TimeTicks& processing_time) {
  ui::EventType event_type;
  switch (event.GetAction()) {
    case ui::MotionEvent::Action::DOWN:
    case ui::MotionEvent::Action::POINTER_DOWN:
      event_type = ui::EventType::kTouchPressed;
      break;
    case ui::MotionEvent::Action::MOVE:
      event_type = ui::EventType::kTouchMoved;
      break;
    case ui::MotionEvent::Action::UP:
    case ui::MotionEvent::Action::POINTER_UP:
      event_type = ui::EventType::kTouchReleased;
      break;
    default:
      return;
  }
  ui::ComputeEventLatencyOS(event_type, event.GetEventTime(), processing_time);
}

}  // namespace input
