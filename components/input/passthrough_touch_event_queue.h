// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_PASSTHROUGH_TOUCH_EVENT_QUEUE_H_
#define COMPONENTS_INPUT_PASSTHROUGH_TOUCH_EVENT_QUEUE_H_

#include <set>
#include <string>

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/input/event_with_latency_info.h"
#include "base/component_export.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "ui/events/blink/blink_features.h"

namespace content {
class InputRouterImplTestBase;
} // namespace content

namespace input {

class TouchTimeoutHandler;

// Interface with which PassthroughTouchEventQueue can forward touch events, and
// dispatch touch event responses.
class COMPONENT_EXPORT(INPUT) PassthroughTouchEventQueueClient {
 public:
  virtual ~PassthroughTouchEventQueueClient() {}

  virtual void SendTouchEventImmediately(
      const TouchEventWithLatencyInfo& event) = 0;

  virtual void OnTouchEventAck(
      const TouchEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) = 0;

  virtual void OnFilteringTouchEvent(
      const blink::WebTouchEvent& touch_event) = 0;

  virtual void FlushDeferredGestureQueue() = 0;
};

// A queue that processes a touch-event and forwards it on to the
// renderer process immediately. This class assumes that queueing will
// happen inside the renderer process. This class will hold onto the pending
// events so that it can re-order the acks so that they appear in a
// logical order to the rest of the browser process. Due to the threaded
// model of the renderer it is possible that an ack for a touchend can
// be sent before the corresponding ack for the touchstart. This class
// corrects that state.
//
// This class also performs filtering over the sequence of touch-events to, for
// example, avoid sending events to the renderer that would have no effect. By
// default, we always forward touchstart and touchend but, if there are no
// handlers, touchmoves are filtered out of the sequence. The filtering logic
// is implemented in |FilterBeforeForwarding|.
class COMPONENT_EXPORT(INPUT) PassthroughTouchEventQueue {
 public:
  struct COMPONENT_EXPORT(INPUT) Config {
    Config();
    ~Config();
    Config(const Config& other);

    // Touch ack timeout delay for desktop sites. If zero, timeout behavior
    // is disabled for such sites. Defaults to 200ms.
    base::TimeDelta desktop_touch_ack_timeout_delay = base::Milliseconds(200);

    // Touch ack timeout delay for mobile sites. If zero, timeout behavior
    // is disabled for such sites. Defaults to 1000ms.
    base::TimeDelta mobile_touch_ack_timeout_delay = base::Milliseconds(1000);

    // Whether the platform supports touch ack timeout behavior.
    // Defaults to false (disabled).
    bool touch_ack_timeout_supported = false;

    // Whether we should allow events to bypass normal queue filter rules.
    bool skip_touch_filter =
        base::FeatureList::IsEnabled(blink::features::kSkipTouchEventFilter);

    // What events types are allowed to bypass the filter.
    std::string events_to_always_forward = kSkipTouchEventFilterType.Get();

    scoped_refptr<base::SequencedTaskRunner> task_runner;
  };

  PassthroughTouchEventQueue(PassthroughTouchEventQueueClient* client,
                             const Config& config);

  PassthroughTouchEventQueue(const PassthroughTouchEventQueue&) = delete;
  PassthroughTouchEventQueue& operator=(const PassthroughTouchEventQueue&) =
      delete;

  ~PassthroughTouchEventQueue();

  void QueueEvent(const TouchEventWithLatencyInfo& event);

  void PrependTouchScrollNotification();

  void ProcessTouchAck(blink::mojom::InputEventResultSource ack_source,
                       blink::mojom::InputEventResultState ack_result,
                       const ui::LatencyInfo& latency_info,
                       const uint32_t unique_touch_event_id,
                       bool should_stop_timeout_monitor);

  void OnGestureEventAck(const GestureEventWithLatencyInfo& event,
                         blink::mojom::InputEventResultState ack_result);

  void OnHasTouchEventHandlers(bool has_handlers);

  bool IsPendingAckTouchStart() const;

  void SetAckTimeoutEnabled(bool enabled);

  void SetIsMobileOptimizedSite(bool mobile_optimized_site);

  bool IsAckTimeoutEnabled() const;

  bool Empty() const;

  void StopTimeoutMonitor();

  // Empties the queue of touch events. This may result in any number of gesture
  // events being sent to the renderer.
  void FlushQueue();

 protected:
  void SendTouchCancelEventForTouchEvent(
      const TouchEventWithLatencyInfo& event_to_cancel);
  void UpdateTouchConsumerStates(
      const blink::WebTouchEvent& event,
      blink::mojom::InputEventResultState ack_result);

 private:
  friend class content::InputRouterImplTestBase;
  friend class PassthroughTouchEventQueueTest;
  FRIEND_TEST_ALL_PREFIXES(PassthroughTouchEventQueueTest,
                           TouchScrollStartedUnfiltered);
  FRIEND_TEST_ALL_PREFIXES(PassthroughTouchEventQueueTest,
                           TouchStartWithoutPageHandlersUnfiltered);
  FRIEND_TEST_ALL_PREFIXES(PassthroughTouchEventQueueTest,
                           TouchStartWithPageHandlersUnfiltered);
  FRIEND_TEST_ALL_PREFIXES(PassthroughTouchEventQueueTest,
                           TouchMoveFilteredAfterTimeout);
  FRIEND_TEST_ALL_PREFIXES(PassthroughTouchEventQueueTest,
                           TouchMoveWithoutPageHandlersUnfiltered);
  FRIEND_TEST_ALL_PREFIXES(PassthroughTouchEventQueueTest,
                           StationaryTouchMoveFiltered);
  FRIEND_TEST_ALL_PREFIXES(PassthroughTouchEventQueueTest,
                           StationaryTouchMoveWithActualTouchMoveUnfiltered);
  FRIEND_TEST_ALL_PREFIXES(PassthroughTouchEventQueueTest,
                           NonTouchMoveUnfiltered);
  FRIEND_TEST_ALL_PREFIXES(PassthroughTouchEventQueueTest,
                           TouchMoveWithNonTouchMoveUnfiltered);
  FRIEND_TEST_ALL_PREFIXES(PassthroughTouchEventQueueTest,
                           TouchMoveWithoutSequenceHandlerUnfiltered);
  FRIEND_TEST_ALL_PREFIXES(PassthroughTouchEventQueueTest,
                           TouchMoveWithoutPageHandlersUnfilteredWithSkipFlag);
  FRIEND_TEST_ALL_PREFIXES(PassthroughTouchEventQueueTest,
                           TouchStartUnfilteredWithForwardDiscrete);
  FRIEND_TEST_ALL_PREFIXES(PassthroughTouchEventQueueTest,
                           TouchMoveFilteredWithForwardDiscrete);
  FRIEND_TEST_ALL_PREFIXES(PassthroughTouchEventQueueTest,
                           TouchStartUnfilteredWithForwardAll);
  FRIEND_TEST_ALL_PREFIXES(PassthroughTouchEventQueueTest,
                           TouchMoveUnfilteredWithForwardAll);

  friend class TouchTimeoutHandler;

  class TouchEventWithLatencyInfoAndAckState
      : public TouchEventWithLatencyInfo {
   public:
    TouchEventWithLatencyInfoAndAckState(
        const TouchEventWithLatencyInfo&);
    blink::mojom::InputEventResultState ack_state() const { return ack_state_; }
    blink::mojom::InputEventResultSource ack_source() const {
      return ack_source_;
    }
    void set_ack_info(blink::mojom::InputEventResultSource source,
                      blink::mojom::InputEventResultState state) {
      ack_source_ = source;
      ack_state_ = state;
    }

   private:
    blink::mojom::InputEventResultSource ack_source_;
    blink::mojom::InputEventResultState ack_state_;
  };

  struct TouchEventWithLatencyInfoAndAckStateComparator {
    using is_transparent = void;
    bool operator()(const TouchEventWithLatencyInfoAndAckState& lhs,
                    const TouchEventWithLatencyInfoAndAckState& rhs) const {
      return lhs.event.unique_touch_event_id < rhs.event.unique_touch_event_id;
    }
    bool operator()(const TouchEventWithLatencyInfoAndAckState& lhs,
                    const uint32_t rhs) const {
      return lhs.event.unique_touch_event_id < rhs;
    }
    bool operator()(const uint32_t lhs,
                    const TouchEventWithLatencyInfoAndAckState& rhs) const {
      return lhs < rhs.event.unique_touch_event_id;
    }
  };

  enum class PreFilterResult {
    kUnfiltered = 0,
    kFilteredNoPageHandlers = 1,
    kFilteredTimeout = 2,
    kFilteredNoNonstationaryPointers = 3,
    kFilteredNoHandlerForSequence = 4,
    kMaxValue = kFilteredNoHandlerForSequence,
  };

  // Filter touches prior to forwarding to the renderer, e.g., if the renderer
  // has no touch handler.
  PreFilterResult FilterBeforeForwarding(const blink::WebTouchEvent& event);
  PreFilterResult FilterBeforeForwardingImpl(const blink::WebTouchEvent& event);
  bool ShouldFilterForEvent(const blink::WebTouchEvent& event);

  void AckTouchEventToClient(
      const TouchEventWithLatencyInfo& acked_event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result);

  void SendTouchEventImmediately(TouchEventWithLatencyInfo* touch,
                                 bool wait_for_ack);

  void AckCompletedEvents();

  bool IsTimeoutRunningForTesting() const;
  size_t SizeForTesting() const;

  // Handles touch event forwarding and ack'ed event dispatch.
  raw_ptr<PassthroughTouchEventQueueClient> client_;

  // Whether the renderer has at least one consumer of touch events, e.g. a JS
  // event handler or hit-testable scrollbars
  bool has_handlers_;

  // Whether any pointer in the touch sequence may have having a consumer.
  bool maybe_has_handler_for_current_sequence_;

  // Whether to allow any remaining touches for the current sequence. Note that
  // this is a stricter condition than an empty |touch_consumer_states_|, as it
  // also prevents forwarding of touchstart events for new pointers in the
  // current sequence. This is only used when the event is synthetically
  // cancelled after a touch timeout or before a portal activation.
  bool drop_remaining_touches_in_sequence_;

  // Optional handler for timed-out touch event acks.
  std::unique_ptr<TouchTimeoutHandler> timeout_handler_;

  // Whether touch events should be sent as uncancelable or not.
  bool send_touch_events_async_;

  bool processing_acks_;

  // Event is saved to compare pointer positions for new touchmove events.
  std::unique_ptr<blink::WebTouchEvent> last_sent_touchevent_;

  // Stores outstanding touches that have been sent to the renderer but have
  // not yet been ack'd by the renderer. The set is explicitly ordered based
  // on the unique touch event id.
  std::set<TouchEventWithLatencyInfoAndAckState,
           TouchEventWithLatencyInfoAndAckStateComparator>
      outstanding_touches_;

  // Whether we should allow events to bypass normal queue filter rules.
  const bool skip_touch_filter_;
  // What events types are allowed to bypass the filter.
  const std::string events_to_always_forward_;
  static const base::FeatureParam<std::string> kSkipTouchEventFilterType;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_PASSTHROUGH_TOUCH_EVENT_QUEUE_H_
