// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_RENDER_INPUT_ROUTER_DELEGATE_H_
#define COMPONENTS_INPUT_RENDER_INPUT_ROUTER_DELEGATE_H_

#include <memory>

#include "cc/trees/render_frame_metadata.h"
#include "components/input/peak_gpu_memory_tracker.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "ui/gfx/delegated_ink_point.h"

namespace input {

class RenderWidgetHostViewInput;
class RenderInputRouterIterator;
class RenderWidgetHostInputEventRouter;
class TouchEmulator;

class COMPONENT_EXPORT(INPUT) RenderInputRouterDelegate {
 public:
  virtual ~RenderInputRouterDelegate() = default;

  virtual RenderWidgetHostViewInput* GetPointerLockView() = 0;
  // TODO(b/331419617): Use a new FrameMetadataBase class instead of
  // RenderFrameMetadata.
  virtual const cc::RenderFrameMetadata& GetLastRenderFrameMetadata() = 0;

  virtual std::unique_ptr<RenderInputRouterIterator>
  GetEmbeddedRenderInputRouters() = 0;

  virtual RenderWidgetHostInputEventRouter* GetInputEventRouter() = 0;

  // Forwards |delegated_ink_point| to viz over IPC to be drawn as part of
  // delegated ink trail, resetting the |ended_delegated_ink_trail| flag.
  virtual void ForwardDelegatedInkPoint(
      gfx::DelegatedInkPoint& delegated_ink_point,
      bool& ended_delegated_ink_trail) = 0;
  // Instructs viz to reset prediction for delegated ink trails, indicating that
  // the trail has ended. Updates the |ended_delegated_ink_trail| flag to
  // reflect this change.
  virtual void ResetDelegatedInkPointPrediction(
      bool& ended_delegated_ink_trail) = 0;

  virtual bool IsIgnoringWebInputEvents(
      const blink::WebInputEvent& event) const = 0;

  virtual bool PreHandleGestureEvent(const blink::WebGestureEvent& event) = 0;

  virtual void NotifyObserversOfInputEvent(
      const blink::WebInputEvent& event) = 0;
  virtual void NotifyObserversOfInputEventAcks(
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result,
      const blink::WebInputEvent& event) = 0;

  // Returns an pointer to the existing touch emulator serving this host if
  // |create_if_necessary| is false. If true, calling this function will force
  // creation of a TouchEmulator.
  virtual TouchEmulator* GetTouchEmulator(bool create_if_necessary) = 0;

  virtual std::unique_ptr<input::PeakGpuMemoryTracker> MakePeakGpuMemoryTracker(
      input::PeakGpuMemoryTracker::Usage usage) = 0;

  // Called upon event ack receipt from the renderer.
  virtual void OnWheelEventAck(
      const input::MouseWheelEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) = 0;

  // Returns true iff the renderer process was initialized and it hasn't died
  // yet.
  virtual bool IsInitializedAndNotDead() = 0;

  // Invoked before an input event is sent to the renderer. This provides the
  // delegate an opportunity to be informed that the input event is being
  // dispatched to the widget.
  virtual void OnInputEventPreDispatch(const blink::WebInputEvent& event) = 0;

  // Called when an invalid input event source is sent from the renderer.
  virtual void OnInvalidInputEventSource() = 0;

  // TODO(b/345483526): Cleanup BrowserPrioritizeNativeWork,
  // BrowserDeferUIThreadTasks experiments.
  virtual void NotifyUISchedulerOfGestureEventUpdate(
      blink::WebInputEvent::Type gesture_event) = 0;

  // Notifies when an input event is ignored, see `IsIgnoringWebInputEvents`
  // above.
  virtual void OnInputIgnored(const blink::WebInputEvent& event) = 0;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_RENDER_INPUT_ROUTER_DELEGATE_H_
