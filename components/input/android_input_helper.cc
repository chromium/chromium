// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/android_input_helper.h"

#include "components/input/events_helper.h"
#include "components/input/render_input_router.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/input/switches.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event_traits.h"

namespace input {

AndroidInputHelper::~AndroidInputHelper() = default;

AndroidInputHelper::AndroidInputHelper(RenderWidgetHostViewInput* view,
                                       Delegate* delegate)
    : view_(*view), delegate_(*delegate) {}

bool AndroidInputHelper::ShouldRouteEvents() const {
  CHECK(view_->GetViewRenderInputRouter());
  return view_->GetViewRenderInputRouter()->delegate() &&
         view_->GetViewRenderInputRouter()->delegate()->GetInputEventRouter();
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

}  // namespace input
