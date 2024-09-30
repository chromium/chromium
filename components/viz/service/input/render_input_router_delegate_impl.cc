
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/render_input_router_delegate_impl.h"

#include <utility>

#include "base/notimplemented.h"
#include "components/input/render_widget_host_input_event_router.h"

namespace viz {

RenderInputRouterDelegateImpl::RenderInputRouterDelegateImpl(
    scoped_refptr<input::RenderWidgetHostInputEventRouter> rwhier,
    const FrameSinkId& frame_sink_id)
    : rwhier_(std::move(rwhier)), frame_sink_id_(frame_sink_id) {
  TRACE_EVENT_INSTANT(
      "input", "RenderInputRouterDelegateImpl::RenderInputRouterDelegateImpl",
      "frame_sink_id", frame_sink_id);
  CHECK(rwhier_);
}

RenderInputRouterDelegateImpl::~RenderInputRouterDelegateImpl() {
  TRACE_EVENT_INSTANT(
      "input", "RenderInputRouterDelegateImpl::~RenderInputRouterDelegateImpl",
      "frame_sink_id", frame_sink_id_);
}

input::RenderWidgetHostViewInput*
RenderInputRouterDelegateImpl::GetPointerLockView() {
  // This is required when we are doing targeting for mouse/mousewheel events.
  // Mouse events are not being handled on Viz with current scope of
  // InputVizard.
  NOTREACHED();
}

const cc::RenderFrameMetadata&
RenderInputRouterDelegateImpl::GetLastRenderFrameMetadata() {
  // TODO(b/365541296): Implement RenderInputRouterDelegate interface in Viz.
  NOTREACHED();
}

std::unique_ptr<input::RenderInputRouterIterator>
RenderInputRouterDelegateImpl::GetEmbeddedRenderInputRouters() {
  // TODO(b/365541296): Implement RenderInputRouterDelegate interface in Viz.
  NOTIMPLEMENTED();
  return nullptr;
}

input::RenderWidgetHostInputEventRouter*
RenderInputRouterDelegateImpl::GetInputEventRouter() {
  return rwhier_.get();
}

bool RenderInputRouterDelegateImpl::IsIgnoringWebInputEvents(
    const blink::WebInputEvent& event) const {
  // TODO(b/365541296): Implement RenderInputRouterDelegate interface in Viz.
  NOTIMPLEMENTED();
  return false;
}

bool RenderInputRouterDelegateImpl::PreHandleGestureEvent(
    const blink::WebGestureEvent& event) {
  return false;
}

void RenderInputRouterDelegateImpl::NotifyObserversOfInputEvent(
    const blink::WebInputEvent& event) {
  // TODO(b/365541296): Implement RenderInputRouterDelegate interface in Viz.
  NOTIMPLEMENTED();
}

bool RenderInputRouterDelegateImpl::IsInitializedAndNotDead() {
  // Since this is being checked in Viz, Renderer process should already have
  // been initialized. When the renderer process dies, |this| will be deleted
  // and sending input to renderer is stopped at InputManager level.
  return true;
}

input::TouchEmulator* RenderInputRouterDelegateImpl::GetTouchEmulator(
    bool create_if_necessary) {
  // Touch emulation is handled solely on browser.
  return nullptr;
}

std::unique_ptr<input::PeakGpuMemoryTracker>
RenderInputRouterDelegateImpl::MakePeakGpuMemoryTracker(
    input::PeakGpuMemoryTracker::Usage usage) {
  // TODO(b/365541296): Implement RenderInputRouterDelegate interface in Viz.
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace viz
