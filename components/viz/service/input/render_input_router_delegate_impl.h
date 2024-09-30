// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_INPUT_RENDER_INPUT_ROUTER_DELEGATE_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_INPUT_RENDER_INPUT_ROUTER_DELEGATE_IMPL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/input/event_with_latency_info.h"
#include "components/input/peak_gpu_memory_tracker.h"
#include "components/input/render_input_router_delegate.h"
#include "components/input/render_input_router_iterator.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/viz_service_export.h"

namespace input {
class RenderWidgetHostViewInput;
class RenderInputRouterIterator;
class RenderWidgetHostInputEventRouter;
class TouchEmulator;
}  // namespace input

namespace viz {

// RenderInputRouterDelegateImpl provides RenderInputRouter access to input
// handling related information and functionality within Viz.
class VIZ_SERVICE_EXPORT RenderInputRouterDelegateImpl
    : public input::RenderInputRouterDelegate {
 public:
  explicit RenderInputRouterDelegateImpl(
      scoped_refptr<input::RenderWidgetHostInputEventRouter> rwhier,
      const FrameSinkId& frame_sink_id);

  ~RenderInputRouterDelegateImpl() override;

  // RenderInputRouterDelegate overrides.
  input::RenderWidgetHostViewInput* GetPointerLockView() override;
  const cc::RenderFrameMetadata& GetLastRenderFrameMetadata() override;
  std::unique_ptr<input::RenderInputRouterIterator>
  GetEmbeddedRenderInputRouters() override;
  input::RenderWidgetHostInputEventRouter* GetInputEventRouter() override;
  void ForwardDelegatedInkPoint(gfx::DelegatedInkPoint& delegated_ink_point,
                                bool& ended_delegated_ink_trail) override {}
  void ResetDelegatedInkPointPrediction(
      bool& ended_delegated_ink_trail) override {}
  bool IsIgnoringWebInputEvents(
      const blink::WebInputEvent& event) const override;
  bool PreHandleGestureEvent(const blink::WebGestureEvent& event) override;
  void NotifyObserversOfInputEvent(const blink::WebInputEvent& event) override;
  void NotifyObserversOfInputEventAcks(
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result,
      const blink::WebInputEvent& event) override {}
  input::TouchEmulator* GetTouchEmulator(bool create_if_necessary) override;
  std::unique_ptr<input::PeakGpuMemoryTracker> MakePeakGpuMemoryTracker(
      input::PeakGpuMemoryTracker::Usage usage) override;
  void OnWheelEventAck(
      const input::MouseWheelEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) override {}
  bool IsInitializedAndNotDead() override;
  void OnInputEventPreDispatch(const blink::WebInputEvent& event) override {}
  void OnInvalidInputEventSource() override {}
  void NotifyUISchedulerOfGestureEventUpdate(
      blink::WebInputEvent::Type gesture_event) override {}
  void OnInputIgnored(const blink::WebInputEvent& event) override {}

 private:
  scoped_refptr<input::RenderWidgetHostInputEventRouter> rwhier_;
  const FrameSinkId frame_sink_id_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_INPUT_RENDER_INPUT_ROUTER_DELEGATE_IMPL_H_
