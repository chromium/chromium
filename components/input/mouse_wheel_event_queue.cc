// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/mouse_wheel_event_queue.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/web_input_event_traits.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseWheelEvent;
using ui::LatencyInfo;

namespace input {

MouseWheelEventQueue::MouseWheelEventQueue(MouseWheelEventQueueClient* client)
    : client_(client),
      send_wheel_events_async_(false),
      scrolling_device_(blink::WebGestureDevice::kUninitialized) {
  DCHECK(client);
}

MouseWheelEventQueue::~MouseWheelEventQueue() {
}

void MouseWheelEventQueue::QueueEvent(
    const MouseWheelEventWithLatencyInfo& event) {
  TRACE_EVENT0("input", "MouseWheelEventQueue::QueueEvent");

  if (event_sent_for_gesture_ack_ && !wheel_queue_.empty()) {
    QueuedWebMouseWheelEvent* last_event = wheel_queue_.back().get();
    if (last_event->CanCoalesceWith(event)) {
      // Terminate the LatencyInfo of the event before it gets coalesced away.
      event.latency.Terminate();

      last_event->CoalesceWith(event);
      // The deltas for the coalesced event change; the corresponding action
      // might be different now.
      last_event->event.event_action =
          WebMouseWheelEvent::GetPlatformSpecificDefaultEventAction(
              last_event->event);
      TRACE_EVENT_INSTANT2("input", "MouseWheelEventQueue::CoalescedWheelEvent",
                           TRACE_EVENT_SCOPE_THREAD, "total_dx",
                           last_event->event.delta_x, "total_dy",
                           last_event->event.delta_y);
      return;
    }
  }

  MouseWheelEventWithLatencyInfo event_with_action(event.event,
                                                          event.latency);
  event_with_action.event.event_action =
      WebMouseWheelEvent::GetPlatformSpecificDefaultEventAction(event.event);
  // Update the expected event action before queuing the event. From this point
  // on, the action should not change.
  wheel_queue_.push_back(
      std::make_unique<QueuedWebMouseWheelEvent>(event_with_action));
  TryForwardNextEventToRenderer();
  LOCAL_HISTOGRAM_COUNTS_100("Renderer.WheelQueueSize", wheel_queue_.size());
}

bool MouseWheelEventQueue::CanGenerateGestureScroll(
    blink::mojom::InputEventResultState ack_result) const {
  if (ack_result == blink::mojom::InputEventResultState::kConsumed) {
    TRACE_EVENT_INSTANT0("input", "Wheel Event Consumed",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (event_sent_for_gesture_ack_->event.event_action ==
      blink::WebMouseWheelEvent::EventAction::kPageZoom) {
    TRACE_EVENT_INSTANT0("input", "Wheel Event Cannot Cause Scroll",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (scrolling_device_ != blink::WebGestureDevice::kUninitialized &&
      scrolling_device_ != blink::WebGestureDevice::kTouchpad) {
    TRACE_EVENT_INSTANT0("input",
                         "Autoscroll or Touchscreen Scroll In Progress",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  // When the cursor has entered the autoscroll mode but no mouse move has
  // arrived yet, We should still ignore wheel scrolling even though no GSB with
  // autoscroll source has been sent yet.
  if (client_->IsAutoscrollInProgress()) {
    TRACE_EVENT_INSTANT0("input", "In Autoscrolling mode",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  return true;
}

void MouseWheelEventQueue::ProcessMouseWheelAck(
    const MouseWheelEventWithLatencyInfo& ack_event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  TRACE_EVENT0("input", "MouseWheelEventQueue::ProcessMouseWheelAck");
  if (!event_sent_for_gesture_ack_)
    return;

  event_sent_for_gesture_ack_->latency.AddNewLatencyFrom(ack_event.latency);
  client_->OnMouseWheelEventAck(*event_sent_for_gesture_ack_, ack_source,
                                ack_result);

  // If event wasn't consumed then generate a gesture scroll for it.
  if (CanGenerateGestureScroll(ack_result)) {
    WebGestureEvent scroll_update(
        WebInputEvent::Type::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
        event_sent_for_gesture_ack_->event.TimeStamp(),
        blink::WebGestureDevice::kTouchpad);

    scroll_update.SetPositionInWidget(
        event_sent_for_gesture_ack_->event.PositionInWidget());
    scroll_update.SetPositionInScreen(
        event_sent_for_gesture_ack_->event.PositionInScreen());

#if !BUILDFLAG(IS_MAC)
    // Swap X & Y if Shift is down and when there is no horizontal movement.
    if (event_sent_for_gesture_ack_->event.event_action ==
            blink::WebMouseWheelEvent::EventAction::kScrollHorizontal &&
        event_sent_for_gesture_ack_->event.delta_x == 0) {
      scroll_update.data.scroll_update.delta_x =
          event_sent_for_gesture_ack_->event.delta_y;
      scroll_update.data.scroll_update.delta_y =
          event_sent_for_gesture_ack_->event.delta_x;
    } else
#endif  // BUILDFLAG(IS_MAC)
    {
      scroll_update.data.scroll_update.delta_x =
          event_sent_for_gesture_ack_->event.delta_x;
      scroll_update.data.scroll_update.delta_y =
          event_sent_for_gesture_ack_->event.delta_y;
    }

    if (event_sent_for_gesture_ack_->event.momentum_phase !=
        blink::WebMouseWheelEvent::kPhaseNone) {
      scroll_update.data.scroll_update.inertial_phase =
          WebGestureEvent::InertialPhaseState::kMomentum;
    } else if (event_sent_for_gesture_ack_->event.phase !=
               blink::WebMouseWheelEvent::kPhaseNone) {
      scroll_update.data.scroll_update.inertial_phase =
          WebGestureEvent::InertialPhaseState::kNonMomentum;
    }

    // WebMouseWheelEvent only supports these units for the delta.
    DCHECK(event_sent_for_gesture_ack_->event.delta_units ==
               ui::ScrollGranularity::kScrollByPage ||
           event_sent_for_gesture_ack_->event.delta_units ==
               ui::ScrollGranularity::kScrollByPrecisePixel ||
           event_sent_for_gesture_ack_->event.delta_units ==
               ui::ScrollGranularity::kScrollByPixel ||
           event_sent_for_gesture_ack_->event.delta_units ==
               ui::ScrollGranularity::kScrollByPercentage);
    scroll_update.data.scroll_update.delta_units =
        event_sent_for_gesture_ack_->event.delta_units;

    if (event_sent_for_gesture_ack_->event.delta_units ==
        ui::ScrollGranularity::kScrollByPage) {
      // Turn page scrolls into a *single* page scroll because
      // the magnitude the number of ticks is lost when coalescing.
      if (scroll_update.data.scroll_update.delta_x)
        scroll_update.data.scroll_update.delta_x =
            scroll_update.data.scroll_update.delta_x > 0 ? 1 : -1;
      if (scroll_update.data.scroll_update.delta_y)
        scroll_update.data.scroll_update.delta_y =
            scroll_update.data.scroll_update.delta_y > 0 ? 1 : -1;
    } else {
      if (event_sent_for_gesture_ack_->event.rails_mode ==
          WebInputEvent::kRailsModeVertical)
        scroll_update.data.scroll_update.delta_x = 0;
      if (event_sent_for_gesture_ack_->event.rails_mode ==
          WebInputEvent::kRailsModeHorizontal)
        scroll_update.data.scroll_update.delta_y = 0;
    }

    bool current_phase_ended = false;
    bool scroll_phase_ended = false;
    bool momentum_phase_ended = false;

    if (event_sent_for_gesture_ack_->event.phase !=
            blink::WebMouseWheelEvent::kPhaseNone ||
        event_sent_for_gesture_ack_->event.momentum_phase !=
            blink::WebMouseWheelEvent::kPhaseNone) {
      scroll_phase_ended = event_sent_for_gesture_ack_->event.phase ==
                               blink::WebMouseWheelEvent::kPhaseEnded ||
                           event_sent_for_gesture_ack_->event.phase ==
                               blink::WebMouseWheelEvent::kPhaseCancelled;
      momentum_phase_ended =
          event_sent_for_gesture_ack_->event.momentum_phase ==
              blink::WebMouseWheelEvent::kPhaseEnded ||
          event_sent_for_gesture_ack_->event.momentum_phase ==
              blink::WebMouseWheelEvent::kPhaseCancelled;
      current_phase_ended = scroll_phase_ended || momentum_phase_ended;
    }

    bool needs_update = scroll_update.data.scroll_update.delta_x != 0 ||
                        scroll_update.data.scroll_update.delta_y != 0;

    bool synthetic = event_sent_for_gesture_ack_->event.has_synthetic_phase;

    // Generally, there should always be a non-zero delta with kPhaseBegan
    // events. However, sometimes this is not the case and the delta in both
    // directions is 0. When this occurs, don't call SendScrollBegin because
    // scroll direction is necessary in order to determine the correct scroller
    // to target and latch to.
    if (needs_update && event_sent_for_gesture_ack_->event.phase ==
                            blink::WebMouseWheelEvent::kPhaseBegan) {
      send_wheel_events_async_ = true;

      if (!client_->IsWheelScrollInProgress())
        SendScrollBegin(scroll_update, synthetic);
    }

    if (needs_update) {
      // It is possible that the wheel event with phaseBegan is consumed and
      // no GSB is sent.
      if (!client_->IsWheelScrollInProgress())
        SendScrollBegin(scroll_update, synthetic);
      client_->ForwardGestureEventWithLatencyInfo(scroll_update,
                                                  ui::LatencyInfo());
    }

    if (current_phase_ended && client_->IsWheelScrollInProgress()) {
      // Send GSE when GSB is sent and no fling is going to happen next.
      SendScrollEnd(scroll_update, synthetic);
    }
  }

  event_sent_for_gesture_ack_.reset();
  TryForwardNextEventToRenderer();
}

void MouseWheelEventQueue::OnGestureScrollEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (gesture_event.event.GetType() ==
      blink::WebInputEvent::Type::kGestureScrollBegin) {
    scrolling_device_ = gesture_event.event.SourceDevice();
  } else if (scrolling_device_ == gesture_event.event.SourceDevice() &&
             gesture_event.event.GetType() ==
                 blink::WebInputEvent::Type::kGestureScrollEnd) {
    scrolling_device_ = blink::WebGestureDevice::kUninitialized;
  } else if (gesture_event.event.GetType() ==
             blink::WebInputEvent::Type::kGestureFlingStart) {
    // With browser side fling we shouldn't reset scrolling_device_ on GFS since
    // the fling_controller processes the GFS to generate and send GSU events.
  }
}

void MouseWheelEventQueue::TryForwardNextEventToRenderer() {
  TRACE_EVENT0("input", "MouseWheelEventQueue::TryForwardNextEventToRenderer");

  if (wheel_queue_.empty() || event_sent_for_gesture_ack_)
    return;

  event_sent_for_gesture_ack_ = std::move(wheel_queue_.front());
  wheel_queue_.pop_front();

  DCHECK(event_sent_for_gesture_ack_->event.phase !=
             blink::WebMouseWheelEvent::kPhaseNone ||
         event_sent_for_gesture_ack_->event.momentum_phase !=
             blink::WebMouseWheelEvent::kPhaseNone);
  if (event_sent_for_gesture_ack_->event.phase ==
      blink::WebMouseWheelEvent::kPhaseBegan) {
    send_wheel_events_async_ = false;
  } else if (send_wheel_events_async_) {
    event_sent_for_gesture_ack_->event.dispatch_type =
        WebInputEvent::DispatchType::kEventNonBlocking;
  }

  client_->SendMouseWheelEventImmediately(
      *event_sent_for_gesture_ack_,
      base::BindOnce(&MouseWheelEventQueue::ProcessMouseWheelAck,
                     base::Unretained(this)));
}

void MouseWheelEventQueue::SendScrollEnd(WebGestureEvent update_event,
                                         bool synthetic) {
  DCHECK(client_->IsWheelScrollInProgress());

  WebGestureEvent scroll_end(update_event);
  scroll_end.SetTimeStamp(ui::EventTimeForNow());
  scroll_end.SetType(WebInputEvent::Type::kGestureScrollEnd);
  scroll_end.data.scroll_end.synthetic = synthetic;
  scroll_end.data.scroll_end.inertial_phase =
      update_event.data.scroll_update.inertial_phase;
  scroll_end.data.scroll_end.delta_units =
      update_event.data.scroll_update.delta_units;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS wheel events with synthetic momentum_phase ==
  // blink::WebMouseWheelEvent::kPhaseEnded are generated by the fling
  // controller to terminate touchpad flings.
  if (scroll_end.data.scroll_end.inertial_phase ==
          WebGestureEvent::InertialPhaseState::kMomentum &&
      synthetic) {
    scroll_end.data.scroll_end.generated_by_fling_controller = true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  client_->ForwardGestureEventWithLatencyInfo(scroll_end, ui::LatencyInfo());
}

void MouseWheelEventQueue::SendScrollBegin(
    const WebGestureEvent& gesture_update,
    bool synthetic) {
  DCHECK(!client_->IsWheelScrollInProgress());

  WebGestureEvent scroll_begin =
      ui::ScrollBeginFromScrollUpdate(gesture_update);
  scroll_begin.data.scroll_begin.synthetic = synthetic;

  client_->ForwardGestureEventWithLatencyInfo(scroll_begin, ui::LatencyInfo());
}

}  // namespace input
