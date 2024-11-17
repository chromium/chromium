// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/render_input_router_support_child_frame.h"

#include "components/input/features.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/viz/service/input/render_input_router_support_base.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"

namespace viz {

RenderInputRouterSupportChildFrame::~RenderInputRouterSupportChildFrame() =
    default;

RenderInputRouterSupportChildFrame::RenderInputRouterSupportChildFrame(
    input::RenderInputRouter* rir,
    RenderInputRouterSupportBase::Delegate* delegate,
    const FrameSinkId& frame_sink_id)
    : RenderInputRouterSupportBase(rir, delegate, frame_sink_id) {
  input_helper_ = std::make_unique<input::ChildFrameInputHelper>(this, this);
  UpdateFrameSinkIdRegistration();
}

const LocalSurfaceId& RenderInputRouterSupportChildFrame::GetLocalSurfaceId()
    const {
  // Not needed on Viz.
  NOTREACHED();
}

RenderInputRouterSupportBase*
RenderInputRouterSupportChildFrame::GetRootView() {
  return delegate()->GetRootRenderInputRouterSupport(GetFrameSinkId());
}

FrameSinkId RenderInputRouterSupportChildFrame::GetRootFrameSinkId() {
  RenderInputRouterSupportBase* root_view = GetRootView();
  if (root_view) {
    return root_view->GetRootFrameSinkId();
  }
  return FrameSinkId();
}

SurfaceId RenderInputRouterSupportChildFrame::GetCurrentSurfaceId() const {
  // Not needed on Viz.
  NOTREACHED();
}

void RenderInputRouterSupportChildFrame::NotifyHitTestRegionUpdated(
    const AggregatedHitTestRegion& region) {
  RenderInputRouterSupportBase::NotifyHitTestRegionUpdated(region);
  input_helper_->NotifyHitTestRegionUpdated(region);
}

bool RenderInputRouterSupportChildFrame::ScreenRectIsUnstableFor(
    const blink::WebInputEvent& event) {
  return input_helper_->ScreenRectIsUnstableFor(event);
}

bool RenderInputRouterSupportChildFrame::ScreenRectIsUnstableForIOv2For(
    const blink::WebInputEvent& event) {
  return input_helper_->ScreenRectIsUnstableForIOv2For(event);
}

void RenderInputRouterSupportChildFrame::PreProcessMouseEvent(
    const blink::WebMouseEvent& event) {
  // Mouse events are not handled currently in Viz with InputVizard.
  NOTREACHED();
}

gfx::PointF
RenderInputRouterSupportChildFrame::TransformRootPointToViewCoordSpace(
    const gfx::PointF& point) {
  return input_helper_->TransformRootPointToViewCoordSpace(point);
}

gfx::PointF RenderInputRouterSupportChildFrame::TransformPointToRootCoordSpaceF(
    const gfx::PointF& point) {
  return input_helper_->TransformPointToRootCoordSpaceF(point);
}

bool RenderInputRouterSupportChildFrame::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    input::RenderWidgetHostViewInput* target_view,
    gfx::PointF* transformed_point) {
  return input_helper_->TransformPointToCoordSpaceForView(point, target_view,
                                                          transformed_point);
}

void RenderInputRouterSupportChildFrame::TransformPointToRootSurface(
    gfx::PointF* point) {
  input_helper_->TransformPointToRootSurface(point);
}

RenderInputRouterSupportBase*
RenderInputRouterSupportChildFrame::GetParentViewInput() {
  return delegate()->GetParentRenderInputRouterSupport(GetFrameSinkId());
}

RenderInputRouterSupportBase*
RenderInputRouterSupportChildFrame::GetRootViewInput() {
  return GetRootView();
}

blink::mojom::InputEventResultState
RenderInputRouterSupportChildFrame::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  return input_helper_->FilterInputEvent(input_event);
}

void RenderInputRouterSupportChildFrame::GestureEventAck(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  TRACE_EVENT1("input", "RenderInputRouterSupportChildFrame::GestureEventAck",
               "type", blink::WebInputEvent::GetName(event.GetType()));
  input_helper_->GestureEventAckHelper(event, ack_source, ack_result);
}

bool RenderInputRouterSupportChildFrame::IsPointerLocked() {
  // Used by mouse and mouse wheel events, which are not in scope for
  // InputVizard currently.
  NOTREACHED();
}

void RenderInputRouterSupportChildFrame::StopFlingingIfNecessary(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  input_helper_->StopFlingingIfNecessary(event, ack_result);
}

void RenderInputRouterSupportChildFrame::ForwardTouchpadZoomEventIfNecessary(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  input_helper_->ForwardTouchpadZoomEventIfNecessary(event, ack_result);
}

}  // namespace viz
