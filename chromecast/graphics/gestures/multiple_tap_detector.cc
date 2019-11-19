// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/gestures/multiple_tap_detector.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"

namespace chromecast {

MultipleTapDetector::MultipleTapDetector(aura::Window* root_window,
                                         MultipleTapDetectorDelegate* delegate)
    : root_window_(root_window),
      delegate_(delegate),
      enabled_(false),
      tap_state_(MultiTapState::NONE),
      tap_count_(0) {
  root_window->GetHost()->GetEventSource()->AddEventRewriter(this);
}

MultipleTapDetector::~MultipleTapDetector() {
  root_window_->GetHost()->GetEventSource()->RemoveEventRewriter(this);
}

ui::EventDispatchDetails MultipleTapDetector::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  if (!enabled_ || !delegate_ || !event.IsTouchEvent()) {
    return SendEvent(continuation, &event);
  }

  const ui::TouchEvent& touch_event = static_cast<const ui::TouchEvent&>(event);
  if (event.type() == ui::ET_TOUCH_PRESSED) {
    // If a press happened again before the minimum inter-tap interval, cancel
    // the detection.
    if (tap_state_ == MultiTapState::INTERVAL_WAIT &&
        (event.time_stamp() - stashed_events_.back().event.time_stamp()) <
            gesture_detector_config_.double_tap_min_time) {
      stashed_events_.clear();
      TapDetectorStateReset();
      return SendEvent(continuation, &event);
    }

    // If the user moved too far from the last tap position, it's not a multi
    // tap.
    if (tap_count_) {
      float distance = (touch_event.location() - last_tap_location_).Length();
      if (distance > gesture_detector_config_.double_tap_slop) {
        TapDetectorStateReset();
        stashed_events_.clear();
        return SendEvent(continuation, &event);
      }
    }

    // Otherwise transition into a touched state.
    tap_state_ = MultiTapState::TOUCH;
    last_tap_location_ = touch_event.location();

    // If this is pressed too long, it should be treated as a long-press, and
    // not part of a triple-tap, so set a timer to detect that.
    triple_tap_timer_.Start(
        FROM_HERE, gesture_detector_config_.longpress_timeout, this,
        &MultipleTapDetector::OnLongPressIntervalTimerFired);

    // If we've already gotten one tap, discard this event, only the original
    // tap needs to get through.
    if (tap_count_) {
      return DiscardEvent(continuation);
    }

    // Copy the event so we can issue a cancel for it later if this turns out to
    // be a multi-tap.
    stashed_events_.emplace_back(touch_event, continuation);

    return SendEvent(continuation, &event);
  }

  // Finger was released while we were waiting for one, count it as a tap.
  if (touch_event.type() == ui::ET_TOUCH_RELEASED &&
      tap_state_ == MultiTapState::TOUCH) {
    tap_state_ = MultiTapState::INTERVAL_WAIT;
    triple_tap_timer_.Start(FROM_HERE,
                            gesture_detector_config_.double_tap_timeout, this,
                            &MultipleTapDetector::OnTapIntervalTimerFired);

    tap_count_++;
    if (tap_count_ == 3) {
      TapDetectorStateReset();
      delegate_->OnTripleTap(touch_event.location());

      // Issue cancel events for old presses.
      ui::EventDispatchDetails details;
      for (const auto& it : stashed_events_) {
        ui::TouchEvent cancel_event(
            ui::ET_TOUCH_CANCELLED, it.event.location_f(),
            it.event.root_location_f(), it.event.time_stamp(),
            it.event.pointer_details(), it.event.flags());
        details = SendEvent(it.continuation, &cancel_event);
        if (details.dispatcher_destroyed)
          break;
      }
      stashed_events_.clear();
      return details;
    } else if (tap_count_ > 1) {
      return DiscardEvent(continuation);
    }
  }

  return SendEvent(continuation, &event);
}

void MultipleTapDetector::OnTapIntervalTimerFired() {
  // We didn't quite reach a third tap, but a second was reached.
  // So call out the double-tap.
  if (tap_count_ == 2) {
    delegate_->OnDoubleTap(last_tap_location_);
    if (!stashed_events_.empty()) {
      Stash& stash = stashed_events_.front();
      ui::TouchEvent cancel_event(
          ui::ET_TOUCH_CANCELLED, stash.event.location_f(),
          stash.event.root_location_f(), base::TimeTicks::Now(),
          stash.event.pointer_details(), stash.event.flags());
      DCHECK(
          !SendEvent(stash.continuation, &cancel_event).dispatcher_destroyed);
    }
  }
  TapDetectorStateReset();
  stashed_events_.clear();
}

void MultipleTapDetector::OnLongPressIntervalTimerFired() {
  TapDetectorStateReset();
  stashed_events_.clear();
}

void MultipleTapDetector::TapDetectorStateReset() {
  tap_state_ = MultiTapState::NONE;
  tap_count_ = 0;
  triple_tap_timer_.Stop();
}

MultipleTapDetector::Stash::Stash(const ui::TouchEvent& e, const Continuation c)
    : event(e), continuation(c) {}

MultipleTapDetector::Stash::~Stash() {}

}  // namespace chromecast
