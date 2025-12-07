// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/render_input_router_delegate_impl.h"

#include <utility>

#include "base/notimplemented.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/viz/service/input/peak_gpu_memory_tracker_impl.h"
#include "ui/latency/latency_info.h"

namespace viz {

namespace {
bool IsInputEventContinuous(const blink::WebInputEvent& event) {
  using Type = blink::mojom::EventType;
  return (event.GetType() == Type::kTouchMove ||
          event.GetType() == Type::kGestureScrollUpdate ||
          event.GetType() == Type::kGesturePinchUpdate);
}

}  // namespace

RenderInputRouterDelegateImpl::RenderInputRouterDelegateImpl(
    scoped_refptr<input::RenderWidgetHostInputEventRouter> rwhier,
    Delegate& delegate,
    const FrameSinkId& frame_sink_id)
    : rwhier_(std::move(rwhier)),
      delegate_(delegate),
      frame_sink_id_(frame_sink_id) {
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

std::optional<bool> RenderInputRouterDelegateImpl::IsDelegatedInkHovering() {
  return delegate_->IsDelegatedInkHovering(frame_sink_id_);
}

std::unique_ptr<input::RenderInputRouterIterator>
RenderInputRouterDelegateImpl::GetEmbeddedRenderInputRouters() {
  return delegate_->GetEmbeddedRenderInputRouters(frame_sink_id_);
}

input::RenderWidgetHostInputEventRouter*
RenderInputRouterDelegateImpl::GetInputEventRouter() {
  return rwhier_.get();
}

bool RenderInputRouterDelegateImpl::IsIgnoringWebInputEvents(
    const blink::WebInputEvent& event) const {
  // When browser starts ignoring input events, it calls
  // RenderWidgetHostViewAndroid::ResetGestureDetection which results in
  // dropping the rest of the current input sequence. If WebContents ignores
  // input events according to WebInputEventAuditCallback, it is applicable from
  // the next input sequence and the current input sequence will not ignore
  // input events on VizCompositorThread.
  return false;
}

bool RenderInputRouterDelegateImpl::PreHandleGestureEvent(
    const blink::WebGestureEvent& event) {
  return false;
}

void RenderInputRouterDelegateImpl::NotifyObserversOfInputEvent(
    const blink::WebInputEvent& event,
    bool dispatched_to_renderer) {
  if (IsInputEventContinuous(event)) {
    return;
  }

  auto* remote = delegate_->GetRIRDelegateClientRemote(frame_sink_id_);
  if (!remote) {
    return;
  }

  auto web_coalesced_event =
      std::make_unique<blink::WebCoalescedInputEvent>(event, ui::LatencyInfo());
  remote->NotifyObserversOfInputEvent(std::move(web_coalesced_event),
                                      dispatched_to_renderer);
}

void RenderInputRouterDelegateImpl::NotifyObserversOfInputEventAcks(
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result,
    const blink::WebInputEvent& event) {
  if (IsInputEventContinuous(event)) {
    return;
  }

  auto* remote = delegate_->GetRIRDelegateClientRemote(frame_sink_id_);
  if (!remote) {
    return;
  }

  auto web_coalesced_event =
      std::make_unique<blink::WebCoalescedInputEvent>(event, ui::LatencyInfo());
  remote->NotifyObserversOfInputEventAcks(ack_source, ack_result,
                                          std::move(web_coalesced_event));
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

void RenderInputRouterDelegateImpl::OnInvalidInputEventSource() {
  auto* remote = delegate_->GetRIRDelegateClientRemote(frame_sink_id_);
  if (!remote) {
    return;
  }
  remote->OnInvalidInputEventSource();
}

std::unique_ptr<PeakGpuMemoryTracker>
RenderInputRouterDelegateImpl::MakePeakGpuMemoryTracker(
    PeakGpuMemoryTracker::Usage usage) {
  return std::make_unique<PeakGpuMemoryTrackerImpl>(usage,
                                                    delegate_->GetGpuService());
}

input::StylusInterface* RenderInputRouterDelegateImpl::GetStylusInterface() {
  // Stylus input is not being handled by InputVizard currently.
  return nullptr;
}

bool RenderInputRouterDelegateImpl::IsHidden() const {
  return is_hidden_;
}

void RenderInputRouterDelegateImpl::OnInputEventAckTimeout(
    base::TimeTicks ack_timeout_ts) {
  if (!is_responsive_) {
    return;
  }
  is_responsive_ = false;
  auto* remote = delegate_->GetRIRDelegateClientRemote(frame_sink_id_);
  if (!remote) {
    return;
  }
  remote->RendererInputResponsivenessChanged(is_responsive_,
                                             std::move(ack_timeout_ts));
}

void RenderInputRouterDelegateImpl::RendererIsResponsive() {
  if (is_responsive_) {
    return;
  }
  is_responsive_ = true;
  auto* remote = delegate_->GetRIRDelegateClientRemote(frame_sink_id_);
  if (!remote) {
    return;
  }
  remote->RendererInputResponsivenessChanged(is_responsive_, std::nullopt);
}

void RenderInputRouterDelegateImpl::DidOverscroll(
    blink::mojom::DidOverscrollParamsPtr params) {
  // |InputRouterImpl::GestureEventHandled| triggers both
  // |RenderInputRouterDelegateImpl::DidOverscroll| (which sends overscroll
  // information to the browser process) and
  // |RenderInputRouterSupportAndroid::GestureEventAck| which calls in
  // StopFlingingIfNecessary, so the decision to stop any fling due to
  // overscroll is handled within the Viz process.
  auto* remote = delegate_->GetRIRDelegateClientRemote(frame_sink_id_);
  if (!remote) {
    return;
  }
  remote->StateOnOverscrollTransfer(std::move(params));
}

}  // namespace viz
