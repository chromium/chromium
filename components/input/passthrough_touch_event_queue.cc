// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/passthrough_touch_event_queue.h"

#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/metrics/field_trial_params.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "components/input/touch_timeout_handler.h"
#include "components/input/web_touch_event_traits.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_f.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using ui::LatencyInfo;

namespace input {
namespace {

// Compare all properties of touch points to determine the state.
bool HasPointChanged(const WebTouchPoint& point_1,
                     const WebTouchPoint& point_2) {
  DCHECK_EQ(point_1.id, point_2.id);
  if (point_1.PositionInScreen() != point_2.PositionInScreen() ||
      point_1.PositionInWidget() != point_2.PositionInWidget() ||
      point_1.radius_x != point_2.radius_x ||
      point_1.radius_y != point_2.radius_y ||
      point_1.rotation_angle != point_2.rotation_angle ||
      point_1.force != point_2.force || point_1.tilt_x != point_2.tilt_x ||
      point_1.tilt_y != point_2.tilt_y) {
    return true;
  }
  return false;
}

}  // namespace

PassthroughTouchEventQueue::Config::Config() = default;
PassthroughTouchEventQueue::Config::~Config() = default;
PassthroughTouchEventQueue::Config::Config(
    const PassthroughTouchEventQueue::Config& other) = default;

// static
const base::FeatureParam<std::string>
    PassthroughTouchEventQueue::kSkipTouchEventFilterType{
        &blink::features::kSkipTouchEventFilter,
        blink::features::kSkipTouchEventFilterTypeParamName,
        blink::features::kSkipTouchEventFilterTypeParamValueDiscrete};

PassthroughTouchEventQueue::TouchEventWithLatencyInfoAndAckState::
    TouchEventWithLatencyInfoAndAckState(
        const TouchEventWithLatencyInfo& event)
    : TouchEventWithLatencyInfo(event),
      ack_state_(blink::mojom::InputEventResultState::kUnknown) {}

PassthroughTouchEventQueue::PassthroughTouchEventQueue(
    PassthroughTouchEventQueueClient* client,
    const Config& config)
    : client_(client),
      has_handlers_(true),
      maybe_has_handler_for_current_sequence_(false),
      drop_remaining_touches_in_sequence_(false),
      send_touch_events_async_(false),
      processing_acks_(false),
      skip_touch_filter_(config.skip_touch_filter),
      events_to_always_forward_(config.events_to_always_forward) {
  if (config.touch_ack_timeout_supported) {
    timeout_handler_ = std::make_unique<TouchTimeoutHandler>(
        this, config.desktop_touch_ack_timeout_delay,
        config.mobile_touch_ack_timeout_delay, config.task_runner);
  }
}

PassthroughTouchEventQueue::~PassthroughTouchEventQueue() {}

void PassthroughTouchEventQueue::SendTouchCancelEventForTouchEvent(
    const TouchEventWithLatencyInfo& event_to_cancel) {
  TouchEventWithLatencyInfo event = event_to_cancel;
  WebTouchEventTraits::ResetTypeAndTouchStates(
      WebInputEvent::Type::kTouchCancel,
      // TODO(rbyers): Shouldn't we use a fresh timestamp?
      event.event.TimeStamp(), &event.event);
  SendTouchEventImmediately(&event, false);
}

void PassthroughTouchEventQueue::QueueEvent(
    const TouchEventWithLatencyInfo& event) {
  TRACE_EVENT0("input", "PassthroughTouchEventQueue::QueueEvent");

  if (FilterBeforeForwarding(event.event) != PreFilterResult::kUnfiltered) {
    client_->OnFilteringTouchEvent(event.event);

    TouchEventWithLatencyInfoAndAckState event_with_ack_state = event;
    event_with_ack_state.set_ack_info(
        blink::mojom::InputEventResultSource::kBrowser,
        blink::mojom::InputEventResultState::kNoConsumerExists);
    outstanding_touches_.insert(event_with_ack_state);
    AckCompletedEvents();
    return;
  }
  TouchEventWithLatencyInfo cloned_event(event);
  SendTouchEventImmediately(&cloned_event, true);
}

void PassthroughTouchEventQueue::PrependTouchScrollNotification() {
  TRACE_EVENT0("input",
               "PassthroughTouchEventQueue::PrependTouchScrollNotification");

  TouchEventWithLatencyInfo touch(
      WebInputEvent::Type::kTouchScrollStarted, WebInputEvent::kNoModifiers,
      ui::EventTimeForNow(), LatencyInfo());
  touch.event.dispatch_type = WebInputEvent::DispatchType::kEventNonBlocking;
  SendTouchEventImmediately(&touch, true);
}

void PassthroughTouchEventQueue::ProcessTouchAck(
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result,
    const LatencyInfo& latency_info,
    const uint32_t unique_touch_event_id,
    bool should_stop_timeout_monitor) {
  TRACE_EVENT0("input", "PassthroughTouchEventQueue::ProcessTouchAck");
  if (timeout_handler_ &&
      timeout_handler_->ConfirmTouchEvent(unique_touch_event_id, ack_result,
                                          should_stop_timeout_monitor))
    return;

  auto touch_event_iter = outstanding_touches_.find(unique_touch_event_id);
  if (touch_event_iter == outstanding_touches_.end()) {
    TRACE_EVENT_INSTANT("input", "unique_touch_event_id NotFound");
    return;
  }

  TouchEventWithLatencyInfoAndAckState& event =
      const_cast<TouchEventWithLatencyInfoAndAckState&>(*touch_event_iter);
  event.latency.AddNewLatencyFrom(latency_info);
  event.set_ack_info(ack_source, ack_result);

  AckCompletedEvents();
}

void PassthroughTouchEventQueue::OnGestureEventAck(
    const GestureEventWithLatencyInfo& event,
    blink::mojom::InputEventResultState ack_result) {
  // When the scroll finishes allow TouchEvents to be blocking again.
  if (event.event.GetType() == blink::WebInputEvent::Type::kGestureScrollEnd) {
    send_touch_events_async_ = false;
  } else if (event.event.GetType() ==
                 blink::WebInputEvent::Type::kGestureScrollUpdate &&
             ack_result == blink::mojom::InputEventResultState::kConsumed) {
    send_touch_events_async_ = true;
  }
}

void PassthroughTouchEventQueue::OnHasTouchEventHandlers(bool has_handlers) {
  has_handlers_ = has_handlers;
}

bool PassthroughTouchEventQueue::IsPendingAckTouchStart() const {
  if (outstanding_touches_.empty())
    return false;

  for (auto& iter : outstanding_touches_) {
    if (iter.event.GetType() == WebInputEvent::Type::kTouchStart)
      return true;
  }
  return false;
}

void PassthroughTouchEventQueue::SetAckTimeoutEnabled(bool enabled) {
  if (timeout_handler_)
    timeout_handler_->SetEnabled(enabled);
}

void PassthroughTouchEventQueue::SetIsMobileOptimizedSite(
    bool mobile_optimized_site) {
  if (timeout_handler_)
    timeout_handler_->SetUseMobileTimeout(mobile_optimized_site);
}

bool PassthroughTouchEventQueue::IsAckTimeoutEnabled() const {
  return timeout_handler_ && timeout_handler_->IsEnabled();
}

bool PassthroughTouchEventQueue::Empty() const {
  return outstanding_touches_.empty();
}

void PassthroughTouchEventQueue::FlushQueue() {
  // Don't allow acks to be processed in AckCompletedEvents as that can
  // interfere with gesture event dispatch ordering.
  base::AutoReset<bool> process_acks(&processing_acks_, true);
  drop_remaining_touches_in_sequence_ = true;
  client_->FlushDeferredGestureQueue();
  while (!outstanding_touches_.empty()) {
    auto iter = outstanding_touches_.begin();
    TouchEventWithLatencyInfoAndAckState event = *iter;
    outstanding_touches_.erase(iter);
    if (event.ack_state() == blink::mojom::InputEventResultState::kUnknown)
      event.set_ack_info(
          blink::mojom::InputEventResultSource::kBrowser,
          blink::mojom::InputEventResultState::kNoConsumerExists);
    AckTouchEventToClient(event, event.ack_source(), event.ack_state());
  }
}

void PassthroughTouchEventQueue::StopTimeoutMonitor() {
  if (timeout_handler_)
    timeout_handler_->StopTimeoutMonitor();
}

void PassthroughTouchEventQueue::AckCompletedEvents() {
  // Don't allow re-entrancy into this method otherwise
  // the ordering of acks won't be preserved.
  if (processing_acks_) {
    TRACE_EVENT_INSTANT("input", "ProcessingAcksAlready");
    return;
  }
  base::AutoReset<bool> process_acks(&processing_acks_, true);
  while (!outstanding_touches_.empty()) {
    auto iter = outstanding_touches_.begin();
    if (iter->ack_state() == blink::mojom::InputEventResultState::kUnknown) {
      TRACE_EVENT_INSTANT("input", "Unknown InputEventResultState");
      break;
    }
    TouchEventWithLatencyInfoAndAckState event = *iter;
    outstanding_touches_.erase(iter);
    AckTouchEventToClient(event, event.ack_source(), event.ack_state());
  }
}

void PassthroughTouchEventQueue::AckTouchEventToClient(
    const TouchEventWithLatencyInfo& acked_event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  UpdateTouchConsumerStates(acked_event.event, ack_result);

  // Skip ack for TouchScrollStarted since it was synthesized within the queue.
  if (acked_event.event.GetType() != WebInputEvent::Type::kTouchScrollStarted) {
    client_->OnTouchEventAck(acked_event, ack_source, ack_result);
  }
}

void PassthroughTouchEventQueue::SendTouchEventImmediately(
    TouchEventWithLatencyInfo* touch,
    bool wait_for_ack) {
  // Note: Touchstart events are marked cancelable to allow transitions between
  // platform scrolling and JS pinching. Touchend events, however, remain
  // uncancelable, mitigating the risk of jank when transitioning to a fling.
  if (send_touch_events_async_ &&
      touch->event.GetType() != WebInputEvent::Type::kTouchStart)
    touch->event.dispatch_type = WebInputEvent::DispatchType::kEventNonBlocking;

  if (touch->event.GetType() == WebInputEvent::Type::kTouchStart)
    touch->event.touch_start_or_first_touch_move = true;

  // For touchmove events, compare touch points position from current event
  // to last sent event and update touch points state.
  if (touch->event.GetType() == WebInputEvent::Type::kTouchMove) {
    CHECK(last_sent_touchevent_);
    if (last_sent_touchevent_->GetType() == WebInputEvent::Type::kTouchStart)
      touch->event.touch_start_or_first_touch_move = true;
    for (unsigned int i = 0; i < last_sent_touchevent_->touches_length; ++i) {
      const WebTouchPoint& last_touch_point = last_sent_touchevent_->touches[i];
      // Touches with same id may not have same index in Touches array.
      for (unsigned int j = 0; j < touch->event.touches_length; ++j) {
        const WebTouchPoint& current_touchmove_point = touch->event.touches[j];
        if (current_touchmove_point.id != last_touch_point.id)
          continue;

        if (!HasPointChanged(last_touch_point, current_touchmove_point))
          touch->event.touches[j].state =
              WebTouchPoint::State::kStateStationary;

        break;
      }
    }
  }

  if (touch->event.GetType() != WebInputEvent::Type::kTouchScrollStarted) {
    if (last_sent_touchevent_)
      *last_sent_touchevent_ = touch->event;
    else
      last_sent_touchevent_ = std::make_unique<WebTouchEvent>(touch->event);
  }

  if (timeout_handler_)
    timeout_handler_->StartIfNecessary(*touch);
  touch->event.GetModifiableEventLatencyMetadata().dispatched_to_renderer =
      base::TimeTicks::Now();
  if (wait_for_ack)
    outstanding_touches_.insert(*touch);
  client_->SendTouchEventImmediately(*touch);
}

PassthroughTouchEventQueue::PreFilterResult
PassthroughTouchEventQueue::FilterBeforeForwarding(const WebTouchEvent& event) {
  PreFilterResult result = FilterBeforeForwardingImpl(event);
  if (result == PreFilterResult::kFilteredTimeout ||
      result == PreFilterResult::kFilteredNoNonstationaryPointers)
    return result;

  // Override non-timeout filter results based on the Finch trial that bypasses
  // the filter. We do this here so that the event still has the opportunity to
  // update any internal state that's necessary to handle future events
  // (i.e. future touch moves might be dropped, even if this touch start isn't
  // due to a filter override).
  if (!ShouldFilterForEvent(event))
    return PreFilterResult::kUnfiltered;

  return result;
}

bool PassthroughTouchEventQueue::ShouldFilterForEvent(
    const blink::WebTouchEvent& event) {
  // Always run all filtering if the SkipTouchEventFilter is disabled.
  if (!skip_touch_filter_)
    return true;
  // If the experiment is enabled and all events are forwarded, always skip
  // filtering.
  if (events_to_always_forward_ ==
      blink::features::kSkipTouchEventFilterTypeParamValueAll)
    return false;
  // If the experiment is enabled and only discrete events are forwarded,
  // always run filtering for touchmove events only.
  return event.GetType() == WebInputEvent::Type::kTouchMove;
}

PassthroughTouchEventQueue::PreFilterResult
PassthroughTouchEventQueue::FilterBeforeForwardingImpl(
    const WebTouchEvent& event) {
  // Unconditionally apply the timeout filter to avoid exacerbating
  // any responsiveness problems on the page.
  if (timeout_handler_ && timeout_handler_->FilterEvent(event))
    return PreFilterResult::kFilteredTimeout;

  if (event.GetType() == WebInputEvent::Type::kTouchScrollStarted)
    return PreFilterResult::kUnfiltered;

  if (event.IsTouchSequenceStart()) {
    // We don't know if we have a handler until we get the ACK back so
    // assume it is true.
    maybe_has_handler_for_current_sequence_ = true;
    send_touch_events_async_ = false;
    last_sent_touchevent_.reset();

    drop_remaining_touches_in_sequence_ = false;
    if (!has_handlers_) {
      drop_remaining_touches_in_sequence_ = true;
      // If the SkipTouchEventFilter experiment is running, drop through to
      // the loop that filters events with no nonstationary pointers below.
      if (ShouldFilterForEvent(event))
        return PreFilterResult::kFilteredNoPageHandlers;
    }
  }

  if (drop_remaining_touches_in_sequence_ &&
      event.GetType() != WebInputEvent::Type::kTouchCancel &&
      // If the SkipTouchEventFilter experiment is running, drop through to
      // the loop that filters events with no nonstationary pointers below.
      ShouldFilterForEvent(event))
    return PreFilterResult::kFilteredNoPageHandlers;

  if (event.GetType() == WebInputEvent::Type::kTouchStart) {
    if (has_handlers_ || maybe_has_handler_for_current_sequence_)
      return PreFilterResult::kUnfiltered;
    // If the SkipTouchEventFilter experiment is running, drop through to
    // the loop that filters events with no nonstationary pointers below.
    else if (ShouldFilterForEvent(event))
      return PreFilterResult::kFilteredNoPageHandlers;
  }

  // If none of the touch points active in the current sequence have handlers,
  // don't forward the touch event.
  if (!maybe_has_handler_for_current_sequence_ && ShouldFilterForEvent(event))
    return PreFilterResult::kFilteredNoHandlerForSequence;

  // Only forward a touch if it has a non-stationary pointer that is active
  // in the current touch sequence.
  for (size_t i = 0; i < event.touches_length; ++i) {
    const WebTouchPoint& point = event.touches[i];
    if (point.state == WebTouchPoint::State::kStateStationary)
      continue;

    // |last_sent_touchevent_| will be non-null as long as there is an
    // active touch sequence being forwarded to the renderer.
    if (!last_sent_touchevent_)
      continue;

    for (size_t j = 0; j < last_sent_touchevent_->touches_length; ++j) {
      if (point.id != last_sent_touchevent_->touches[j].id)
        continue;

      if (event.GetType() != WebInputEvent::Type::kTouchMove)
        return PreFilterResult::kUnfiltered;

      // All pointers in TouchMove events may have state as StateMoved,
      // even though none of the pointers have not changed in real.
      // Forward these events when at least one pointer has changed.
      if (HasPointChanged(last_sent_touchevent_->touches[j], point))
        return PreFilterResult::kUnfiltered;

      // This is a TouchMove event for which we have yet to find a
      // non-stationary pointer. Continue checking the next pointers
      // in the |event|.
      break;
    }
  }

  return PreFilterResult::kFilteredNoNonstationaryPointers;
}

void PassthroughTouchEventQueue::UpdateTouchConsumerStates(
    const WebTouchEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  if (event.GetType() == WebInputEvent::Type::kTouchStart) {
    if (ack_result == blink::mojom::InputEventResultState::kConsumed)
      send_touch_events_async_ = false;

    // Once we have the ack back for the sequence we know if there
    // is a handler or not. Other touch-starts sent can upgrade
    // whether we have a handler or not as well.
    if (event.IsTouchSequenceStart()) {
      maybe_has_handler_for_current_sequence_ =
          ack_result != blink::mojom::InputEventResultState::kNoConsumerExists;
    } else {
      maybe_has_handler_for_current_sequence_ |=
          ack_result != blink::mojom::InputEventResultState::kNoConsumerExists;
    }
  } else if (event.IsTouchSequenceEnd()) {
    maybe_has_handler_for_current_sequence_ = false;
  }
}

size_t PassthroughTouchEventQueue::SizeForTesting() const {
  return outstanding_touches_.size();
}

bool PassthroughTouchEventQueue::IsTimeoutRunningForTesting() const {
  return timeout_handler_ && timeout_handler_->IsTimeoutTimerRunning();
}

}  // namespace input
