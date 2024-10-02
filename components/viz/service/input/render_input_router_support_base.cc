// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/render_input_router_support_base.h"

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "components/input/render_widget_host_input_event_router.h"

namespace viz {

RenderInputRouterSupportBase::~RenderInputRouterSupportBase() {
  NotifyObserversAboutShutdown();
}

RenderInputRouterSupportBase::RenderInputRouterSupportBase(
    input::RenderInputRouter* rir,
    Delegate* delegate,
    const FrameSinkId& frame_sink_id)
    : frame_sink_id_(frame_sink_id), delegate_(delegate), rir_(*rir) {
  TRACE_EVENT_INSTANT(
      "input", "RenderInputRouterSupportBase::RenderInputRouterSupportBase",
      "frame_sink_id", frame_sink_id);
  CHECK(delegate_);
}

bool RenderInputRouterSupportBase::ShouldInitiateStylusWriting() {
  // Stylus input events are not going to be handled on VizCompositor thread
  // with the current scope of InputVizard.
  NOTREACHED();
}

void RenderInputRouterSupportBase::NotifyHoverActionStylusWritable(
    bool stylus_writable) {
  // Stylus input events are not going to be handled on VizCompositor thread
  // with the current scope of InputVizard.
  NOTREACHED();
}

base::WeakPtr<input::RenderWidgetHostViewInput>
RenderInputRouterSupportBase::GetInputWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

input::RenderInputRouter*
RenderInputRouterSupportBase::GetViewRenderInputRouter() {
  return &*rir_;
}

void RenderInputRouterSupportBase::ProcessMouseEvent(
    const blink::WebMouseEvent& event,
    const ui::LatencyInfo& latency) {
  // Mouse events are not handled on VizCompositorThread with current scope of
  // InputVizard.
  NOTREACHED();
}

void RenderInputRouterSupportBase::ProcessMouseWheelEvent(
    const blink::WebMouseWheelEvent& event,
    const ui::LatencyInfo& latency) {
  // MouseWheel events are not handled on VizCompositorThread with current scope
  // of InputVizard.
  NOTREACHED();
}

void RenderInputRouterSupportBase::ProcessTouchEvent(
    const blink::WebTouchEvent& event,
    const ui::LatencyInfo& latency) {
  // Events are not routed to a prerendered page since we don't create RIR's for
  // them, hence we can directly process all input events here. See
  // `RenderWidgetHostViewBase::ProcessTouchEvent` for more details.
  PreProcessTouchEvent(event);
  rir_->ForwardTouchEventWithLatencyInfo(event, latency);
}

void RenderInputRouterSupportBase::ProcessGestureEvent(
    const blink::WebGestureEvent& event,
    const ui::LatencyInfo& latency) {
  // Events are not routed to a prerendered page since we don't create RIR's for
  // them, hence we can directly process all input events here. See
  // `RenderWidgetHostViewBase::ProcessGestureEvent` for more details.
  rir_->ForwardGestureEventWithLatencyInfo(event, latency);
}

RenderInputRouterSupportBase* RenderInputRouterSupportBase::GetRootView() {
  return this;
}

const FrameSinkId& RenderInputRouterSupportBase::GetFrameSinkId() const {
  return frame_sink_id_;
}

void RenderInputRouterSupportBase::OnAutoscrollStart() {
  // Related to mouse events handling which on VizCompositor which is out of
  // scope currently for InputVizard.
  NOTREACHED();
}

float RenderInputRouterSupportBase::GetDeviceScaleFactor() const {
  return delegate_->GetDeviceScaleFactorForId(GetFrameSinkId());
}

bool RenderInputRouterSupportBase::IsPointerLocked() {
  // Related to mouse events handling which on VizCompositor which is out of
  // scope currently for InputVizard.
  NOTREACHED();
}

void RenderInputRouterSupportBase::UpdateFrameSinkIdRegistration() {
  // Let the page-level input event router know about our frame sink ID for
  // surface-based hit testing.
  auto* router = GetViewRenderInputRouter()->delegate()->GetInputEventRouter();
  if (!router->IsViewInMap(this)) {
    router->AddFrameSinkIdOwner(GetFrameSinkId(), this);
  }
}

}  // namespace viz
