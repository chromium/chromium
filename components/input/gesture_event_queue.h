// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_GESTURE_EVENT_QUEUE_H_
#define COMPONENTS_INPUT_GESTURE_EVENT_QUEUE_H_

#include <stddef.h>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/input/event_with_latency_info.h"
#include "components/input/fling_controller.h"
#include "base/component_export.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"

namespace content {
class MockRenderWidgetHost;
}  // namespace content

namespace input {
class GestureEventQueueTest;

// Interface with which the GestureEventQueue can forward gesture events, and
// dispatch gesture event responses.
class COMPONENT_EXPORT(INPUT) GestureEventQueueClient {
 public:
  virtual ~GestureEventQueueClient() {}

  virtual void SendGestureEventImmediately(
      const GestureEventWithLatencyInfo& event) = 0;

  virtual void OnGestureEventAck(
      const GestureEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) = 0;
};

// Despite its name, this class isn't so much one queue as it is a collection
// of queues and filters. This class applies logic to determine if an event
// should be queued, filtered altogether, or sent immediately; it tracks sent
// events and ACKs them to the clilent in the order they were dispatched. This
// class applies a series of filters and queues for various scenarios:
// 1. The sequence is filtered for bounces. A bounce is when the finger lifts
//    from the screen briefly during an in-progress scroll. If this happens,
//    non-GestureScrollUpdate events are queued until the de-bounce interval
//    passes or another GestureScrollUpdate event occurs.
// 2. Unnecessary GestureFlingCancel events are filtered by fling controller.
//    These are GestureFlingCancels that have no corresponding GestureFlingStart
//    in the queue. GestureFlingStarts are also filtered and translated to
//    scroll gestures by the fling controller.
// 3. Taps immediately after a GestureFlingCancel (caused by the same tap) are
//    filtered by fling controller.
// 4. Gesture events are queued while we're waiting to determine the allowed
//    touch actions.
// Sent events are kept in a queue until a response from the renderer is
// received for that event. The client is notified of ACKs in the order the
// events were sent, not ACK'd. This means an ACK'd event that was sent after
// an event still awaiting an ACK won't notify the client until the earlier
// event is ACK'd.
class COMPONENT_EXPORT(INPUT) GestureEventQueue {
 public:
  using GestureQueue = base::circular_deque<GestureEventWithLatencyInfo>;
  struct COMPONENT_EXPORT(INPUT) Config {
    Config();

    FlingController::Config fling_config;

    // Determines whether non-scroll gesture events are "debounced" during an
    // active scroll sequence, suppressing brief scroll interruptions.
    // Zero by default (disabled).
    base::TimeDelta debounce_interval;
  };

  // Both |client| and |touchpad_client| must outlive the GestureEventQueue.
  GestureEventQueue(
      GestureEventQueueClient* client,
      FlingControllerEventSenderClient* fling_event_sender_client,
      FlingControllerSchedulerClient* fling_scheduler_client,
      const Config& config);

  GestureEventQueue(const GestureEventQueue&) = delete;
  GestureEventQueue& operator=(const GestureEventQueue&) = delete;

  ~GestureEventQueue();

  // Allow the fling controller to observe the gesture event. Returns true if
  // the event was filtered by the fling controller and shouldn't be further
  // forwarded.
  bool PassToFlingController(const GestureEventWithLatencyInfo&);

  // Filter the event for debouncing or forward it to the renderer. Returns
  // true if the event was forwarded, false if was filtered for debouncing.
  bool DebounceOrForwardEvent(const GestureEventWithLatencyInfo&);

  // Adds a gesture to the queue of events that needs to be deferred until the
  // touch action is known.
  void QueueDeferredEvents(const GestureEventWithLatencyInfo&);

  // Returns events in the |deferred_gesture_queue_| and empty the queue.
  GestureQueue TakeDeferredEvents();

  // Indicates that the caller has received an acknowledgement from the renderer
  // with state |ack_result| and event |type|.
  void ProcessGestureAck(blink::mojom::InputEventResultSource ack_source,
                         blink::mojom::InputEventResultState ack_result,
                         blink::WebInputEvent::Type type,
                         const ui::LatencyInfo& latency);

  // Returns the |TouchpadTapSuppressionController| instance.
  TouchpadTapSuppressionController*
  GetTouchpadTapSuppressionController();

  // Sends the gesture event to the renderer. Stores the sent event for when
  // the renderer replies with an ACK.
  void ForwardGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event);

  bool empty() const {
    return sent_events_awaiting_ack_.empty() &&
           debouncing_deferral_queue_.empty();
  }

  // Calls |fling_controller_.StopFling| to halt an active fling if such exists.
  void StopFling();

  gfx::Vector2dF CurrentFlingVelocity() const;

  void set_debounce_interval_time_ms_for_testing(int interval_ms) {
    debounce_interval_ = base::Milliseconds(interval_ms);
  }

  // TODO(nburris): Wheel event acks shouldn't really go through the gesture
  // event queue, but this is needed to pass them through to the
  // FlingController. The FlingController should probably be owned by the
  // InputRouter instead.
  void OnWheelEventAck(const MouseWheelEventWithLatencyInfo& event,
                       blink::mojom::InputEventResultSource ack_source,
                       blink::mojom::InputEventResultState ack_result);

  bool IsFlingActiveForTest() { return FlingInProgressForTest(); }

 private:
  friend class GestureEventQueueTest;
  friend class MockRenderWidgetHost;

  class GestureEventWithLatencyInfoAckState
      : public GestureEventWithLatencyInfo {
   public:
    GestureEventWithLatencyInfoAckState(
        const GestureEventWithLatencyInfo&);
    blink::mojom::InputEventResultState ack_state() const { return ack_state_; }
    void set_ack_info(blink::mojom::InputEventResultSource source,
                      blink::mojom::InputEventResultState state) {
      ack_source_ = source;
      ack_state_ = state;
    }
    blink::mojom::InputEventResultSource ack_source() const {
      return ack_source_;
    }

   private:
    blink::mojom::InputEventResultSource ack_source_ =
        blink::mojom::InputEventResultSource::kUnknown;
    blink::mojom::InputEventResultState ack_state_ =
        blink::mojom::InputEventResultState::kUnknown;
  };

  // Inovked on the expiration of the debounce interval to release
  // deferred events.
  void SendScrollEndingEventsNow();

  // Sub-filter for removing bounces from in-progress scrolls.
  bool ShouldForwardForBounceReduction(
      const GestureEventWithLatencyInfo& gesture_event);

  // ACK completed events in order until we have reached an incomplete event.
  // Will preserve the FIFO order as events originally arrived.
  void AckCompletedEvents();
  void AckGestureEventToClient(const GestureEventWithLatencyInfo&,
                               blink::mojom::InputEventResultSource,
                               blink::mojom::InputEventResultState);

  bool FlingInProgressForTest() const;

  // The receiver of all forwarded gesture events.
  raw_ptr<GestureEventQueueClient> client_;

  // True if a GestureScrollUpdate sequence is in progress.
  bool scrolling_in_progress_;

  bool processing_acks_ = false;

  using GestureQueueWithAckState =
      base::circular_deque<GestureEventWithLatencyInfoAckState>;

  // Stores outstanding events that have been sent to the renderer but not yet
  // been ACK'd. These are kept in the order they were sent in so that they can
  // be ACK'd back in order. Note, the renderer can reply to these out-of-order.
  // This class makes a note of the ACK state but doesn't actually let the
  // client know about the ACK until all events earlier in the queue have been
  // ACK'd so that the client sees the ACKs in order.
  GestureQueueWithAckState sent_events_awaiting_ack_;

  // Timer to release a previously deferred gesture event.
  base::OneShotTimer debounce_deferring_timer_;

  // Queue of events that have been deferred for debounce.
  GestureQueue debouncing_deferral_queue_;

  // Queue of gesture events that have been deferred until the main thread touch
  // action is known.
  GestureQueue deferred_gesture_queue_;

  // Time window in which to debounce scroll/fling ends. Note that an interval
  // of zero effectively disables debouncing.
  base::TimeDelta debounce_interval_;

  // An object for filtering unnecessary GFC events, as well as gestureTap/mouse
  // events that happen immediately after touchscreen/touchpad fling canceling
  // taps.
  FlingController fling_controller_;

  // True when the last GSE event is either in the debouncing_deferral_queue_ or
  // pushed to the queue and dropped from it later on.
  bool scroll_end_filtered_by_deboucing_deferral_queue_ = false;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_GESTURE_EVENT_QUEUE_H_
