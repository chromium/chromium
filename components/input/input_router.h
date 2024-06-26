// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_INPUT_ROUTER_H_
#define COMPONENTS_INPUT_INPUT_ROUTER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "cc/input/touch_action.h"
#include "components/input/event_with_latency_info.h"
#include "components/input/gesture_event_queue.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/input/passthrough_touch_event_queue.h"
#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom.h"
#include "third_party/blink/public/mojom/input/touch_event.mojom.h"

namespace input {

// The InputRouter allows the embedder to customize how input events are
// sent to the renderer, and how responses are dispatched to the browser.
// While the router should respect the relative order in which events are
// received, it is free to customize when those events are dispatched.
class InputRouter {
 public:
  struct COMPONENT_EXPORT(INPUT) Config {
    Config();
    GestureEventQueue::Config gesture_config;
    PassthroughTouchEventQueue::Config touch_config;
  };

  virtual ~InputRouter() = default;

  // Note: if the event is processed immediately, the supplied callback is run
  // *synchronously*. If |this| is destroyed while waiting on a result from
  // the renderer, then callbacks are *not* run.
  using MouseEventCallback =
      base::OnceCallback<void(const MouseEventWithLatencyInfo& event,
                              blink::mojom::InputEventResultSource ack_source,
                              blink::mojom::InputEventResultState ack_result)>;
  virtual void SendMouseEvent(
      const MouseEventWithLatencyInfo& mouse_event,
      MouseEventCallback event_result_callback) = 0;

  virtual void SendWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) = 0;

  using KeyboardEventCallback = base::OnceCallback<void(
      const NativeWebKeyboardEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result)>;
  virtual void SendKeyboardEvent(
      const NativeWebKeyboardEventWithLatencyInfo& key_event,
      KeyboardEventCallback event_result_callback) = 0;

  virtual void SendGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) = 0;

  virtual void SendTouchEvent(
      const TouchEventWithLatencyInfo& touch_event) = 0;

  // Notify the router about whether the current page is mobile-optimized (i.e.,
  // the site has a mobile-friendly viewport).
  virtual void NotifySiteIsMobileOptimized(bool is_mobile_optimized) = 0;

  // Whether there are any events pending dispatch to or ack from the renderer.
  virtual bool HasPendingEvents() const = 0;

  // A scale factor to scale the coordinate in WebInputEvent from DIP
  // to viewport.
  virtual void SetDeviceScaleFactor(float device_scale_factor) = 0;

  // Return the currently allowed touch-action.
  virtual std::optional<cc::TouchAction> AllowedTouchAction() = 0;

  // Return the currently active touch-action.
  virtual std::optional<cc::TouchAction> ActiveTouchAction() = 0;

  virtual void SetForceEnableZoom(bool enabled) = 0;

  // Create and bind a new host channel.
  virtual mojo::PendingRemote<blink::mojom::WidgetInputHandlerHost> BindNewHost(
      scoped_refptr<base::SequencedTaskRunner> task_runner) = 0;

  // Used to stop an active fling if such exists.
  virtual void StopFling() = 0;

  // In the case when a gesture event is bubbled from a child frame to the main
  // frame, we set the touch action in the main frame Auto even if there is no
  // pending touch start.
  virtual void ForceSetTouchActionAuto() = 0;

  // Called when the renderer notifies a change in whether or not it has touch
  // event handlers registered.
  virtual void OnHasTouchEventConsumers(
      blink::mojom::TouchEventConsumersPtr consumers) = 0;

  // Will resolve the given callback once all prior input has been fully
  // propagated through the system such that subsequent input will be subject
  // to its effects. e.g. Input that follows a scroll gesture that affects
  // OOPIF hit-testing will need to wait until updated CompositorFrames have
  // been submitted to the browser.
  virtual void WaitForInputProcessed(base::OnceClosure callback) = 0;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_INPUT_ROUTER_H_
