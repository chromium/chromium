// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/touch_timeout_handler.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "components/input/passthrough_touch_event_queue.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_f.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using ui::LatencyInfo;

namespace input {
namespace {

bool ShouldTouchTriggerTimeout(const WebTouchEvent& event) {
  return (event.GetType() == WebInputEvent::Type::kTouchStart ||
          event.GetType() == WebInputEvent::Type::kTouchMove) &&
         event.dispatch_type == WebInputEvent::DispatchType::kBlocking;
}

}  // namespace

TouchTimeoutHandler::TouchTimeoutHandler(
    PassthroughTouchEventQueue* touch_queue,
    base::TimeDelta desktop_timeout_delay,
    base::TimeDelta mobile_timeout_delay,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : touch_queue_(touch_queue),
      desktop_timeout_delay_(desktop_timeout_delay),
      mobile_timeout_delay_(mobile_timeout_delay),
      use_mobile_timeout_(false),
      pending_ack_state_(PENDING_ACK_NONE),
      timeout_monitor_(base::BindRepeating(&TouchTimeoutHandler::OnTimeOut,
                                           base::Unretained(this)),
                       task_runner),
      enabled_(true),
      enabled_for_current_sequence_(false),
      sequence_awaiting_uma_update_(false),
      sequence_using_mobile_timeout_(false) {
  SetUseMobileTimeout(false);
}

TouchTimeoutHandler::~TouchTimeoutHandler() {
  LogSequenceEndForUMAIfNecessary(false);
}

void TouchTimeoutHandler::StartIfNecessary(
    const TouchEventWithLatencyInfo& event) {
  if (pending_ack_state_ != PENDING_ACK_NONE)
    return;

  if (!enabled_)
    return;

  const base::TimeDelta timeout_delay = GetTimeoutDelay();
  if (timeout_delay.is_zero())
    return;

  if (!ShouldTouchTriggerTimeout(event.event))
    return;

  if (event.event.IsTouchSequenceStart()) {
    LogSequenceStartForUMA();
    enabled_for_current_sequence_ = true;
  }

  if (!enabled_for_current_sequence_)
    return;

  timeout_event_ = event;
  timeout_monitor_.Restart(timeout_delay);
}

bool TouchTimeoutHandler::ConfirmTouchEvent(
    uint32_t unique_touch_event_id,
    blink::mojom::InputEventResultState ack_result,
    bool should_stop_timeout_monitor) {
  if (timeout_event_.event.unique_touch_event_id != unique_touch_event_id)
    return false;

  switch (pending_ack_state_) {
    case PENDING_ACK_NONE:
      if (ack_result == blink::mojom::InputEventResultState::kConsumed)
        enabled_for_current_sequence_ = false;
      if (should_stop_timeout_monitor)
        timeout_monitor_.Stop();
      return false;
    case PENDING_ACK_ORIGINAL_EVENT:
      if (AckedTimeoutEventRequiresCancel(ack_result)) {
        TRACE_EVENT_INSTANT("input", "PendingAckOriginalEvent-RequiresCancel");
        SetPendingAckState(PENDING_ACK_CANCEL_EVENT);
        touch_queue_->SendTouchCancelEventForTouchEvent(timeout_event_);
      } else {
        TRACE_EVENT_INSTANT("input", "PendingAckOriginalEvent");
        SetPendingAckState(PENDING_ACK_NONE);
        touch_queue_->UpdateTouchConsumerStates(timeout_event_.event,
                                                ack_result);
      }
      return true;
    case PENDING_ACK_CANCEL_EVENT:
      TRACE_EVENT_INSTANT("input", "PendingAckCancelEvent");
      SetPendingAckState(PENDING_ACK_NONE);
      return true;
  }
  return false;
}

bool TouchTimeoutHandler::FilterEvent(const WebTouchEvent& event) {
  if (!HasTimeoutEvent())
    return false;

  if (event.IsTouchSequenceStart()) {
    // If a new sequence is observed while we're still waiting on the
    // timed-out sequence response, also count the new sequence as timed-out.
    LogSequenceStartForUMA();
    LogSequenceEndForUMAIfNecessary(true);
  }

  return true;
}

void TouchTimeoutHandler::StopTimeoutMonitor() {
  timeout_monitor_.Stop();
}

void TouchTimeoutHandler::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;

  enabled_ = enabled;

  if (enabled_)
    return;

  enabled_for_current_sequence_ = false;
  // Only reset the |timeout_handler_| if the timer is running and has not
  // yet timed out. This ensures that an already timed out sequence is
  // properly flushed by the handler.
  if (IsTimeoutTimerRunning()) {
    pending_ack_state_ = PENDING_ACK_NONE;
    timeout_monitor_.Stop();
  }
}

void TouchTimeoutHandler::SetUseMobileTimeout(bool use_mobile_timeout) {
  use_mobile_timeout_ = use_mobile_timeout;
}

void TouchTimeoutHandler::OnTimeOut() {
  LogSequenceEndForUMAIfNecessary(true);
  SetPendingAckState(PENDING_ACK_ORIGINAL_EVENT);
  touch_queue_->FlushQueue();
}

// Skip a cancel event if the timed-out event had no consumer and was the
// initial event in the gesture.
bool TouchTimeoutHandler::AckedTimeoutEventRequiresCancel(
    blink::mojom::InputEventResultState ack_result) const {
  DCHECK(HasTimeoutEvent());
  if (ack_result != blink::mojom::InputEventResultState::kNoConsumerExists)
    return true;
  return !timeout_event_.event.IsTouchSequenceStart();
}

void TouchTimeoutHandler::SetPendingAckState(
    PendingAckState new_pending_ack_state) {
  DCHECK_NE(pending_ack_state_, new_pending_ack_state);
  switch (new_pending_ack_state) {
    case PENDING_ACK_ORIGINAL_EVENT:
      DCHECK_EQ(pending_ack_state_, PENDING_ACK_NONE);
      TRACE_EVENT_ASYNC_BEGIN0("input", "TouchEventTimeout", this);
      break;
    case PENDING_ACK_CANCEL_EVENT:
      DCHECK_EQ(pending_ack_state_, PENDING_ACK_ORIGINAL_EVENT);
      DCHECK(!timeout_monitor_.IsRunning());
      DCHECK(touch_queue_->Empty());
      TRACE_EVENT_ASYNC_STEP_INTO0("input", "TouchEventTimeout", this,
                                   "CancelEvent");
      break;
    case PENDING_ACK_NONE:
      DCHECK(!timeout_monitor_.IsRunning());
      DCHECK(touch_queue_->Empty());
      TRACE_EVENT_ASYNC_END0("input", "TouchEventTimeout", this);
      break;
  }
  pending_ack_state_ = new_pending_ack_state;
}

void TouchTimeoutHandler::LogSequenceStartForUMA() {
  // Always flush any unlogged entries before starting a new one.
  LogSequenceEndForUMAIfNecessary(false);
  sequence_awaiting_uma_update_ = true;
  sequence_using_mobile_timeout_ = use_mobile_timeout_;
}

void TouchTimeoutHandler::LogSequenceEndForUMAIfNecessary(bool timed_out) {
  if (!sequence_awaiting_uma_update_)
    return;

  sequence_awaiting_uma_update_ = false;

  if (sequence_using_mobile_timeout_) {
    UMA_HISTOGRAM_BOOLEAN("Event.Touch.TimedOutOnMobileSite", timed_out);
  } else {
    UMA_HISTOGRAM_BOOLEAN("Event.Touch.TimedOutOnDesktopSite", timed_out);
  }
}

base::TimeDelta TouchTimeoutHandler::GetTimeoutDelay() const {
  return use_mobile_timeout_ ? mobile_timeout_delay_ : desktop_timeout_delay_;
}

bool TouchTimeoutHandler::HasTimeoutEvent() const {
  return pending_ack_state_ != PENDING_ACK_NONE;
}

}  // namespace input
