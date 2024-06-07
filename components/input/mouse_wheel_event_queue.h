// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_MOUSE_WHEEL_EVENT_QUEUE_H_
#define COMPONENTS_INPUT_MOUSE_WHEEL_EVENT_QUEUE_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/trace_event.h"
#include "components/input/event_with_latency_info.h"
#include "base/component_export.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"

namespace input {

// This class represents a single queued mouse wheel event. Its main use
// is that it is reported via trace events.
class QueuedWebMouseWheelEvent : public MouseWheelEventWithLatencyInfo {
 public:
  QueuedWebMouseWheelEvent(
      const MouseWheelEventWithLatencyInfo& original_event)
      : MouseWheelEventWithLatencyInfo(original_event) {
    TRACE_EVENT_ASYNC_BEGIN0("input", "MouseWheelEventQueue::QueueEvent", this);
  }

  QueuedWebMouseWheelEvent(const QueuedWebMouseWheelEvent&) = delete;
  QueuedWebMouseWheelEvent& operator=(const QueuedWebMouseWheelEvent&) = delete;

  ~QueuedWebMouseWheelEvent() {
    TRACE_EVENT_ASYNC_END0("input", "MouseWheelEventQueue::QueueEvent", this);
  }
};

// Interface with which MouseWheelEventQueue can forward mouse wheel events,
// and dispatch mouse wheel event responses.
class COMPONENT_EXPORT(INPUT) MouseWheelEventQueueClient {
 public:
  using MouseWheelEventHandledCallback = base::OnceCallback<void(
      const MouseWheelEventWithLatencyInfo& ack_event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result)>;
  virtual ~MouseWheelEventQueueClient() {}
  virtual void SendMouseWheelEventImmediately(
      const MouseWheelEventWithLatencyInfo& event,
      MouseWheelEventHandledCallback callback) = 0;
  virtual void OnMouseWheelEventAck(
      const MouseWheelEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) = 0;
  virtual void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& event,
      const ui::LatencyInfo& latency_info) = 0;
  virtual bool IsWheelScrollInProgress() = 0;
  virtual bool IsAutoscrollInProgress() = 0;
};

// A queue for throttling and coalescing mouse wheel events. This class tracks
// wheel events sent to the renderer and receives their ACKs. If the ACK
// reports the event went unconsumed by the renderer, this class will generate
// a sequence of gesture scroll events.
//
// Within a sequence, wheel events are initially forwarded using a blocking
// dispatch. This means that further wheel events are queued and scroll event
// generation will wait (i.e.  block) until the in-flight wheel event is ACKed.
// Once a wheel event goes unconsumed, and scrolling begins, dispatch of
// subsequent wheel events becomes non-blocking. This means the wheel event
// will be ACKed by the browser immediately after being dispatched. This will
// cause scroll events to follow the wheel immediately and new wheel events
// will be dispatched immediately rather than queueing.
class COMPONENT_EXPORT(INPUT) MouseWheelEventQueue {
 public:
  // The |client| must outlive the MouseWheelEventQueue.
  // |IsWheelScrollInProgress| indicates whether mouse wheel events should
  // generate Scroll[Begin|Update|End] on unhandled acknowledge events.
  MouseWheelEventQueue(MouseWheelEventQueueClient* client);

  MouseWheelEventQueue(const MouseWheelEventQueue&) = delete;
  MouseWheelEventQueue& operator=(const MouseWheelEventQueue&) = delete;

  ~MouseWheelEventQueue();

  // Adds an event to the queue. The event may be coalesced with previously
  // queued events (e.g. consecutive mouse-wheel events can be coalesced into a
  // single mouse-wheel event). The event may also be immediately forwarded to
  // the renderer (e.g. when there are no other queued mouse-wheel event).
  void QueueEvent(const MouseWheelEventWithLatencyInfo& event);

  // When GestureScrollBegin is received, and it is a different source
  // than mouse wheels terminate the current GestureScroll if there is one.
  // When Gesture{ScrollEnd,FlingStart} is received, resume generating
  // gestures.
  void OnGestureScrollEvent(
      const GestureEventWithLatencyInfo& gesture_event);

  [[nodiscard]] bool has_pending() const {
    return !wheel_queue_.empty() || event_sent_for_gesture_ack_;
  }

  size_t queued_size() const { return wheel_queue_.size(); }
  bool event_in_flight() const {
    return event_sent_for_gesture_ack_ != nullptr;
  }

 private:
  // Notifies the queue that a mouse wheel event has been processed by the
  // renderer.
  void ProcessMouseWheelAck(
      const MouseWheelEventWithLatencyInfo& ack_event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result);

  void TryForwardNextEventToRenderer();
  void SendScrollEnd(blink::WebGestureEvent update_event, bool synthetic);
  void SendScrollBegin(const blink::WebGestureEvent& gesture_update,
                       bool synthetic);

  // True if gesture scroll events can be generated for the wheel event sent for
  // ack.
  bool CanGenerateGestureScroll(
      blink::mojom::InputEventResultState ack_result) const;

  raw_ptr<MouseWheelEventQueueClient> client_;

  base::circular_deque<std::unique_ptr<QueuedWebMouseWheelEvent>> wheel_queue_;
  std::unique_ptr<QueuedWebMouseWheelEvent> event_sent_for_gesture_ack_;

  // True if the ack for the first wheel event in a scroll sequence is not
  // consumed. This lets us to send the rest of the wheel events in the sequence
  // as non-blocking.
  bool send_wheel_events_async_;

  blink::WebGestureDevice scrolling_device_;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_MOUSE_WHEEL_EVENT_QUEUE_H_
