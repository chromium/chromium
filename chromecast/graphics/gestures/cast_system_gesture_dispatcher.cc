// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/gestures/cast_system_gesture_dispatcher.h"

#include "base/logging.h"
#include "base/time/default_tick_clock.h"

namespace chromecast {

namespace {
const base::TimeDelta kExpirationTime = base::Seconds(3);
const size_t kMaxSwipes = 3;
}  // namespace

CastSystemGestureDispatcher::CastSystemGestureDispatcher(
    const base::TickClock* tick_clock)
    : send_gestures_to_root_(false), tick_clock_(tick_clock) {}

CastSystemGestureDispatcher::CastSystemGestureDispatcher()
    : CastSystemGestureDispatcher(base::DefaultTickClock::GetInstance()) {}

CastSystemGestureDispatcher::~CastSystemGestureDispatcher() {
  DCHECK(gesture_handlers_.empty());
}

void CastSystemGestureDispatcher::AddGestureHandler(
    CastGestureHandler* handler) {
  gesture_handlers_.insert(handler);
}

void CastSystemGestureDispatcher::RemoveGestureHandler(
    CastGestureHandler* handler) {
  gesture_handlers_.erase(handler);
}

CastGestureHandler::Priority CastSystemGestureDispatcher::GetPriority() {
  return Priority::MAX;
}

bool CastSystemGestureDispatcher::CanHandleSwipe(
    CastSideSwipeOrigin swipe_origin) {
  for (auto* gesture_handler : gesture_handlers_) {
    if (gesture_handler->CanHandleSwipe(swipe_origin)) {
      return true;
    }
  }
  return false;
}

void CastSystemGestureDispatcher::HandleSideSwipe(
    CastSideSwipeEvent event,
    CastSideSwipeOrigin swipe_origin,
    const gfx::Point& touch_location) {
  // Process previous events and check to see if the user attempted a swipe
  // multiple times. This probably indicates that the swipe is not having the
  // intended effect in the UI, most likely the highest priority handler is
  // consuming the gesture but not taking any action. To prevent the system
  // from getting stuck, route new gesture events to the main UI when this
  // happens.
  base::TimeTicks now = tick_clock_->NowTicks();
  if (event == CastSideSwipeEvent::BEGIN &&
      swipe_origin == CastSideSwipeOrigin::LEFT) {
    recent_events_.push({now, swipe_origin});
    // Flush events which are older than the prescribed time.
    while (!recent_events_.empty() &&
           recent_events_.front().event_time < now - kExpirationTime) {
      recent_events_.pop();
    }
    // If there are too many recent swipes, then this gesture should go to the
    // root UI.
    send_gestures_to_root_ = recent_events_.size() >= kMaxSwipes;
    if (send_gestures_to_root_) {
      LOG(INFO) << "User swiped " << kMaxSwipes << " times within "
                << kExpirationTime
                << ", sending next swipe gesture to root UI.";
    }
  }
  CastGestureHandler* best_handler = nullptr;
  Priority highest_priority = Priority::NONE;
  // Iterate through all handlers. Pick the handler with the highest priority
  // that is capable of handling the swipe event and is not Priority::NONE.
  for (auto* gesture_handler : gesture_handlers_) {
    if (send_gestures_to_root_ &&
        gesture_handler->GetPriority() == Priority::ROOT_UI) {
      best_handler = gesture_handler;
      break;
    }
    if (gesture_handler->CanHandleSwipe(swipe_origin) &&
        gesture_handler->GetPriority() > highest_priority) {
      best_handler = gesture_handler;
      highest_priority = gesture_handler->GetPriority();
    }
  }
  if (best_handler)
    best_handler->HandleSideSwipe(event, swipe_origin, touch_location);
  if (send_gestures_to_root_ && event == CastSideSwipeEvent::END) {
    // Reset the recent events.
    std::queue<GestureEvent> empty;
    std::swap(recent_events_, empty);
    send_gestures_to_root_ = false;
  }
}

void CastSystemGestureDispatcher::HandleTapDownGesture(
    const gfx::Point& touch_location) {
  for (auto* gesture_handler : gesture_handlers_) {
    gesture_handler->HandleTapDownGesture(touch_location);
  }
}

void CastSystemGestureDispatcher::HandleTapGesture(
    const gfx::Point& touch_location) {
  for (auto* gesture_handler : gesture_handlers_) {
    gesture_handler->HandleTapGesture(touch_location);
  }
}

}  // namespace chromecast
