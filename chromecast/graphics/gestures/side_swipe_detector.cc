// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/gestures/side_swipe_detector.h"

#include <deque>

#include "base/auto_reset.h"
#include "chromecast/base/chromecast_switches.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace chromecast {
namespace {
// The number of pixels from the very left or right of the screen to consider as
// a valid origin for the left or right swipe gesture.
constexpr int kDefaultSideGestureStartWidth = 35;

// The number of pixels from the very top or bottom of the screen to consider as
// a valid origin for the top or bottom swipe gesture.
constexpr int kDefaultSideGestureStartHeight = 35;

// The amount of time after gesture start to allow events which occur in the
// margin to be stashed and replayed within. For example a tap event which
// occurs inside the gesture margin will be valid as long as it occurs within
// the time specified by this threshold.
constexpr base::TimeDelta kGestureMarginEventsTimeLimit =
    base::Milliseconds(500);

// Get the correct bottom gesture start height by checking both margin flags in
// order, and then the default value if neither is set.
int BottomGestureStartHeight() {
  return GetSwitchValueInt(
      switches::kBottomSystemGestureStartHeight,
      GetSwitchValueInt(switches::kSystemGestureStartHeight,
                        kDefaultSideGestureStartHeight));
}

}  // namespace

SideSwipeDetector::SideSwipeDetector(CastGestureHandler* gesture_handler,
                                     aura::Window* root_window)
    : gesture_start_width_(GetSwitchValueInt(switches::kSystemGestureStartWidth,
                                             kDefaultSideGestureStartWidth)),
      gesture_start_height_(
          GetSwitchValueInt(switches::kSystemGestureStartHeight,
                            kDefaultSideGestureStartHeight)),
      bottom_gesture_start_height_(BottomGestureStartHeight()),
      gesture_handler_(gesture_handler),
      root_window_(root_window),
      current_swipe_(CastSideSwipeOrigin::NONE),
      current_pointer_id_(ui::kPointerIdUnknown) {
  DCHECK(gesture_handler);
  DCHECK(root_window);
  root_window_->GetHost()->GetEventSource()->AddEventRewriter(this);
}

SideSwipeDetector::~SideSwipeDetector() {
  root_window_->GetHost()->GetEventSource()->RemoveEventRewriter(this);
}

CastSideSwipeOrigin SideSwipeDetector::GetDragPosition(
    const gfx::Point& point,
    const gfx::Rect& screen_bounds) const {
  if (point.y() < (screen_bounds.y() + gesture_start_height_)) {
    return CastSideSwipeOrigin::TOP;
  }
  if (point.x() < (screen_bounds.x() + gesture_start_width_)) {
    return CastSideSwipeOrigin::LEFT;
  }
  if (point.x() >
      (screen_bounds.x() + screen_bounds.width() - gesture_start_width_)) {
    return CastSideSwipeOrigin::RIGHT;
  }
  if (point.y() > (screen_bounds.y() + screen_bounds.height() -
                   bottom_gesture_start_height_)) {
    return CastSideSwipeOrigin::BOTTOM;
  }
  return CastSideSwipeOrigin::NONE;
}

void SideSwipeDetector::StashEvent(const ui::TouchEvent& event) {
  // If the time since the gesture start is longer than our threshold, do not
  // stash the event (and clear the stashed events).
  if (current_swipe_time_.Elapsed() > kGestureMarginEventsTimeLimit) {
    stashed_events_.clear();
    return;
  }

  stashed_events_.push_back(event);
}

ui::EventDispatchDetails SideSwipeDetector::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  if (!event.IsTouchEvent()) {
    return SendEvent(continuation, &event);
  }

  const ui::TouchEvent* touch_event = event.AsTouchEvent();

  // Touch events come through in screen pixels, but untransformed. This is the
  // raw coordinate not yet mapped to the root window's coordinate system or the
  // screen. Convert it into the root window's coordinate system, in DIP which
  // is what the rest of this class expects.
  gfx::Point touch_location = touch_event->root_location();
  root_window_->GetHost()->ConvertPixelsToDIP(&touch_location);
  gfx::Rect screen_bounds = display::Screen::GetScreen()
                                ->GetDisplayNearestPoint(touch_location)
                                .bounds();
  CastSideSwipeOrigin side_swipe_origin =
      GetDragPosition(touch_location, screen_bounds);

  // A located event has occurred inside the margin. It might be the start of
  // our gesture, or a touch that we need to squash.
  if (current_swipe_ == CastSideSwipeOrigin::NONE &&
      side_swipe_origin != CastSideSwipeOrigin::NONE) {
    // Check to see if we have any potential consumers of events on this side.
    // If not, we can continue on without consuming it.
    if (!gesture_handler_->CanHandleSwipe(side_swipe_origin)) {
      return SendEvent(continuation, &event);
    }

    // Detect the beginning of a system gesture swipe.
    if (touch_event->type() != ui::EventType::kTouchPressed) {
      return SendEvent(continuation, &event);
    }

    current_swipe_ = side_swipe_origin;
    current_pointer_id_ = touch_event->pointer_details().id;

    // Let the subscribers know about the gesture begin.
    gesture_handler_->HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                                      side_swipe_origin, touch_location);

    DVLOG(1) << "side swipe gesture begin @ " << touch_location.ToString();
    current_swipe_time_ = base::ElapsedTimer();

    // Stash a copy of the event should we decide to reconstitute it later if we
    // decide that this isn't in fact a side swipe.
    StashEvent(*touch_event);

    // Avoid corrupt gesture state caused by a missing kGestureScrollEnd event
    // as we potentially transition between web views.
    root_window_->CleanupGestureState();

    // And then stop the original event from propagating.
    return DiscardEvent(continuation);
  }

  // If no swipe in progress, just move on.
  if (current_swipe_ == CastSideSwipeOrigin::NONE) {
    return SendEvent(continuation, &event);
  }

  // If the finger involved is not the one we're looking for, discard it.
  if (touch_event->pointer_details().id != current_pointer_id_) {
    return DiscardEvent(continuation);
  }

  // A swipe is in progress, or has completed, so stop propagation of underlying
  // gesture/touch events, after stashing a copy of the original event.
  StashEvent(*touch_event);

  // The finger has lifted, which means the end of the gesture, or if the finger
  // hasn't travelled far enough, replay the original events.
  if (touch_event->type() == ui::EventType::kTouchReleased) {
    DVLOG(1) << "gesture release; time since press: "
             << current_swipe_time_.Elapsed().InMilliseconds() << "ms @ "
             << touch_location.ToString();
    gesture_handler_->HandleSideSwipe(CastSideSwipeEvent::END, current_swipe_,
                                      touch_location);
    current_swipe_ = CastSideSwipeOrigin::NONE;
    current_pointer_id_ = ui::kPointerIdUnknown;

    // If the finger is still inside the touch margin at release, this is not
    // really a side swipe. Stream out events we stashed for later retrieval.
    if (side_swipe_origin != CastSideSwipeOrigin::NONE &&
        !stashed_events_.empty()) {
      ui::EventDispatchDetails details;
      for (const auto& it : stashed_events_) {
        details = SendEvent(continuation, &it);
        if (details.dispatcher_destroyed)
          break;
      }
      stashed_events_.clear();
      return details;
    }

    // Otherwise, clear them.
    stashed_events_.clear();
    return DiscardEvent(continuation);
  }

  // The system gesture is ongoing...
  gesture_handler_->HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                                    current_swipe_, touch_location);
  DVLOG(1) << "gesture continue; time since press: "
           << current_swipe_time_.Elapsed().InMilliseconds() << "ms @ "
           << touch_location.ToString();

  return DiscardEvent(continuation);
}

}  // namespace chromecast
