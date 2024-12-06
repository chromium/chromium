// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/render_input_router_support_android.h"

#include "base/trace_event/trace_event.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"

namespace viz {

RenderInputRouterSupportAndroid::~RenderInputRouterSupportAndroid() = default;

RenderInputRouterSupportAndroid::RenderInputRouterSupportAndroid(
    input::RenderInputRouter* rir,
    RenderInputRouterSupportBase::Delegate* delegate,
    const FrameSinkId& frame_sink_id)
    : RenderInputRouterSupportBase(rir, delegate, frame_sink_id),
      gesture_provider_(ui::GetGestureProviderConfig(
                            ui::GestureProviderConfigType::CURRENT_PLATFORM,
                            base::SingleThreadTaskRunner::GetCurrentDefault()),
                        this) {
  input_helper_ = std::make_unique<input::AndroidInputHelper>(this, this);
  UpdateFrameSinkIdRegistration();
}

bool RenderInputRouterSupportAndroid::OnTouchEvent(
    const ui::MotionEventAndroid& event,
    bool emit_histograms) {
  if (emit_histograms) {
    input_helper_->RecordToolTypeForActionDown(event);
    input_helper_->ComputeEventLatencyOSTouchHistograms(event);
  }

  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(event);
  if (!result.succeeded) {
    return false;
  }

  blink::WebTouchEvent web_event = ui::CreateWebTouchEventFromMotionEvent(
      event, result.moved_beyond_slop_region /* may_cause_scrolling */,
      false /* hovering */);
  if (web_event.GetType() == blink::WebInputEvent::Type::kUndefined) {
    return false;
  }

  input_helper_->RouteOrForwardTouchEvent(web_event);

  return true;
}

bool RenderInputRouterSupportAndroid::ShouldRouteEvents() const {
  return input_helper_->ShouldRouteEvents();
}

bool RenderInputRouterSupportAndroid::RequiresDoubleTapGestureEvents() const {
  return input_helper_->RequiresDoubleTapGestureEvents();
}

void RenderInputRouterSupportAndroid::OnGestureEvent(
    const ui::GestureEventData& gesture) {
  input_helper_->OnGestureEvent(gesture);
}

void RenderInputRouterSupportAndroid::SendGestureEvent(
    const blink::WebGestureEvent& event) {
  // TODO(365985685): Refactor OverscrollController to work with input on Viz.
  // TODO(366000885): Make touch selection controller input event observer.

  if (event.GetType() == blink::WebInputEvent::Type::kUndefined) {
    return;
  }
  input_helper_->RouteOrForwardGestureEvent(event);
}

ui::FilteredGestureProvider&
RenderInputRouterSupportAndroid::GetGestureProvider() {
  return gesture_provider_;
}

void RenderInputRouterSupportAndroid::ProcessAckedTouchEvent(
    const input::TouchEventWithLatencyInfo& touch,
    blink::mojom::InputEventResultState ack_result) {
  TRACE_EVENT0("input",
               "RenderInputRouterSupportAndroid::ProcessAckedTouchEvent");
  input_helper_->ProcessAckedTouchEvent(touch, ack_result);
}

void RenderInputRouterSupportAndroid::DidOverscroll(
    const ui::DidOverscrollParams& params) {
  // TODO(365985685): Refactor OverscrollController to work with input on Viz.
  NOTREACHED();
}

FrameSinkId RenderInputRouterSupportAndroid::GetRootFrameSinkId() {
  return delegate()->GetRootCompositorFrameSinkId(GetFrameSinkId());
}

SurfaceId RenderInputRouterSupportAndroid::GetCurrentSurfaceId() const {
  // This is not required to be implemented on Viz.
  NOTREACHED();
}

bool RenderInputRouterSupportAndroid::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    input::RenderWidgetHostViewInput* target_view,
    gfx::PointF* transformed_point) {
  return input_helper_->TransformPointToCoordSpaceForView(point, target_view,
                                                          transformed_point);
}

void RenderInputRouterSupportAndroid::TransformPointToRootSurface(
    gfx::PointF* point) {
  auto* metadata = delegate()->GetLastActivatedFrameMetadata(GetFrameSinkId());
  // Adjust the point's y-coordinate to account for the height of the top
  // controls bar. In general, Viz should have the updated top controls height
  // when transform is called and if it isn't updated, the toolbar isn't visible
  // and thus the |point| is already "transformed".
  if (metadata) {
    *point +=
        gfx::Vector2d(0, metadata->top_controls_visible_height.value_or(0.f));
  }
}

blink::mojom::InputEventResultState
RenderInputRouterSupportAndroid::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  // TODO(365985685): Refactor OverscrollController to work with input on Viz.
  NOTREACHED();
}

void RenderInputRouterSupportAndroid::GestureEventAck(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  // TODO(365985685): Refactor OverscrollController to work with input on Viz.

  // Stop flinging if a GSU event with momentum phase is sent to the renderer
  // but not consumed.
  StopFlingingIfNecessary(event, ack_result);
}

}  // namespace viz
