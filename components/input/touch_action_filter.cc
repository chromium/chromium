// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/touch_action_filter.h"

#include <math.h>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/blink/blink_features.h"

using blink::WebInputEvent;
using blink::WebGestureEvent;

namespace input {
namespace {

// Actions on an axis are disallowed if the perpendicular axis has a filter set
// and no filter is set for the queried axis.
bool IsYAxisActionDisallowed(cc::TouchAction action) {
  return ((action & cc::TouchAction::kPanX) != cc::TouchAction::kNone) &&
         ((action & cc::TouchAction::kPanY) == cc::TouchAction::kNone);
}

bool IsXAxisActionDisallowed(cc::TouchAction action) {
  return ((action & cc::TouchAction::kPanY) != cc::TouchAction::kNone) &&
         ((action & cc::TouchAction::kPanX) == cc::TouchAction::kNone);
}

void SetCursorControlIfNecessary(WebGestureEvent* event,
                                 cc::TouchAction action) {
  if (event->data.scroll_begin.pointer_count != 1)
    return;
  const float abs_delta_x = fabs(event->data.scroll_begin.delta_x_hint);
  const float abs_delta_y = fabs(event->data.scroll_begin.delta_y_hint);
  if (abs_delta_x <= abs_delta_y)
    return;

  // We shouldn't reach here if kPanX is not allowed for horizontal scroll.
  DCHECK_NE(action & cc::TouchAction::kPanX, cc::TouchAction::kNone);
  if ((action & cc::TouchAction::kInternalPanXScrolls) ==
      cc::TouchAction::kInternalPanXScrolls)
    return;

  event->data.scroll_begin.cursor_control = true;
}

}  // namespace

TouchActionFilter::TouchActionFilter() {
  ResetTouchAction();
}

TouchActionFilter::~TouchActionFilter() {}

FilterGestureEventResult TouchActionFilter::FilterGestureEvent(
    WebGestureEvent* gesture_event) {
  TRACE_EVENT0("input", "TouchActionFilter::FilterGestureEvent");
  if (gesture_event->SourceDevice() != blink::WebGestureDevice::kTouchscreen)
    return FilterGestureEventResult::kAllowed;

  if (has_deferred_events_) {
    TRACE_EVENT_INSTANT0("input", "Has Deferred", TRACE_EVENT_SCOPE_THREAD);
    return FilterGestureEventResult::kDelayed;
  }

  TRACE_EVENT_INSTANT1(
      "input", "active_action", TRACE_EVENT_SCOPE_THREAD, "action",
      (active_touch_action_.has_value()
           ? cc::TouchActionToString(active_touch_action_.value())
           : "n/a"));
  TRACE_EVENT_INSTANT1(
      "input", "allowed_action", TRACE_EVENT_SCOPE_THREAD, "action",
      (allowed_touch_action_.has_value()
           ? cc::TouchActionToString(allowed_touch_action_.value())
           : "n/a"));
  TRACE_EVENT_INSTANT1(
      "input", "compositor_allowed_action", TRACE_EVENT_SCOPE_THREAD, "action",
      cc::TouchActionToString(compositor_allowed_touch_action_));

  cc::TouchAction touch_action = active_touch_action_.has_value()
                                     ? active_touch_action_.value()
                                     : compositor_allowed_touch_action_;

  // Filter for allowable touch actions first (eg. before the TouchEventQueue
  // can decide to send a touch cancel event).
  switch (gesture_event->GetType()) {
    case WebInputEvent::Type::kGestureScrollBegin: {
      // In VR or virtual keyboard (https://crbug.com/880701),
      // GestureScrollBegin could come without GestureTapDown.
      // TODO(bokan): This can also happen due to the fling controller
      // filtering out the GestureTapDown due to tap suppression (i.e. tapping
      // during a fling should stop the fling, not be sent to the page). We
      // should not reset the touch action in this case! We currently work
      // around this by resetting the compositor allowed touch action in this
      // case as well but we should investigate not filtering the TapDown.
      if (!gesture_sequence_in_progress_) {
        TRACE_EVENT_INSTANT0("input", "No Sequence at GSB!",
                             TRACE_EVENT_SCOPE_THREAD);
        gesture_sequence_in_progress_ = true;
        if (allowed_touch_action_.has_value()) {
          active_touch_action_ = allowed_touch_action_;
          touch_action = allowed_touch_action_.value();
        } else {
          touch_action = compositor_allowed_touch_action_;
        }
      }
      drop_scroll_events_ = ShouldSuppressScrolling(
          *gesture_event, touch_action, active_touch_action_.has_value());
      FilterGestureEventResult res;
      if (!drop_scroll_events_) {
        if (allow_cursor_control_) {
          SetCursorControlIfNecessary(gesture_event, touch_action);
        }
        res = FilterGestureEventResult::kAllowed;
      } else if (active_touch_action_.has_value()) {
        res = FilterGestureEventResult::kFiltered;
      } else {
        TRACE_EVENT_INSTANT0("input", "Deferring Events",
                             TRACE_EVENT_SCOPE_THREAD);
        has_deferred_events_ = true;
        res = FilterGestureEventResult::kDelayed;
      }
      return res;
    }

    case WebInputEvent::Type::kGestureScrollUpdate: {
      if (drop_scroll_events_) {
        TRACE_EVENT_INSTANT0("input", "Drop Events", TRACE_EVENT_SCOPE_THREAD);
        return FilterGestureEventResult::kFiltered;
      }

      // Scrolls restricted to a specific axis shouldn't permit movement
      // in the perpendicular axis.
      //
      // Note the direction suppression with pinch-zoom here, which matches
      // Edge: a "touch-action: pan-y pinch-zoom" region allows vertical
      // two-finger scrolling but a "touch-action: pan-x pinch-zoom" region
      // doesn't.
      // TODO(mustaq): Add it to spec?
      if (IsYAxisActionDisallowed(touch_action)) {
        if (!active_touch_action_.has_value() &&
            gesture_event->data.scroll_update.delta_y != 0) {
          TRACE_EVENT_INSTANT0("input", "Defer Due to YAxis",
                               TRACE_EVENT_SCOPE_THREAD);
          has_deferred_events_ = true;
          return FilterGestureEventResult::kDelayed;
        }
        gesture_event->data.scroll_update.delta_y = 0;
      } else if (IsXAxisActionDisallowed(touch_action)) {
        if (!active_touch_action_.has_value() &&
            gesture_event->data.scroll_update.delta_x != 0) {
          TRACE_EVENT_INSTANT0("input", "Defer Due to XAxis",
                               TRACE_EVENT_SCOPE_THREAD);
          has_deferred_events_ = true;
          return FilterGestureEventResult::kDelayed;
        }
        gesture_event->data.scroll_update.delta_x = 0;
      }
      break;
    }

    case WebInputEvent::Type::kGestureFlingStart:
      // Fling controller processes FlingStart event, and we should never get
      // it here.
      NOTREACHED_IN_MIGRATION();
      break;

    case WebInputEvent::Type::kGestureScrollEnd:
      // Do not reset |compositor_allowed_touch_action_|. In the fling cancel
      // case, the ack for the second touch sequence start, which sets the
      // compositor allowed touch action, could arrive before the GSE of the
      // first fling sequence, we do not want to reset the compositor allowed
      // touch action.
      gesture_sequence_in_progress_ = false;
      return FilterScrollEventAndResetState();

    // Evaluate the |drop_pinch_events_| here instead of GSB because pinch
    // events could arrive without GSB, e.g. double-tap-drag.
    case WebInputEvent::Type::kGesturePinchBegin:
      drop_pinch_events_ = (touch_action & cc::TouchAction::kPinchZoom) ==
                           cc::TouchAction::kNone;
      [[fallthrough]];
    case WebInputEvent::Type::kGesturePinchUpdate:
      if (!drop_pinch_events_)
        return FilterGestureEventResult::kAllowed;
      if (!active_touch_action_.has_value()) {
        has_deferred_events_ = true;
        return FilterGestureEventResult::kDelayed;
      }
      return FilterGestureEventResult::kFiltered;
    case WebInputEvent::Type::kGesturePinchEnd:
      return FilterPinchEventAndResetState();

    // The double tap gesture is a tap ending event. If a double-tap gesture is
    // filtered out, replace it with a tap event but preserve the tap-count to
    // allow firing dblclick event in Blink.
    //
    // TODO(mustaq): This replacement of a double-tap gesture with a tap seems
    // buggy, it produces an inconsistent gesture event stream: GestureTapCancel
    // followed by GestureTap.  See crbug.com/874474#c47 for a repro.  We don't
    // know of any bug resulting from it, but it's better to fix the broken
    // assumption here at least to avoid introducing new bugs in future.
    case WebInputEvent::Type::kGestureDoubleTap:
      gesture_sequence_in_progress_ = false;
      DCHECK_EQ(1, gesture_event->data.tap.tap_count);
      if (!allow_current_double_tap_event_) {
        gesture_event->SetType(WebInputEvent::Type::kGestureTap);
        gesture_event->data.tap.tap_count = 2;
      }
      allow_current_double_tap_event_ = true;
      break;

    // If double tap is disabled, there's no reason for the tap delay.
    case WebInputEvent::Type::kGestureTapUnconfirmed: {
      DCHECK_EQ(1, gesture_event->data.tap.tap_count);
      allow_current_double_tap_event_ =
          (touch_action & cc::TouchAction::kDoubleTapZoom) !=
          cc::TouchAction::kNone;
      if (!allow_current_double_tap_event_) {
        gesture_event->SetType(WebInputEvent::Type::kGestureTap);
        drop_current_tap_ending_event_ = true;
      }
      break;
    }

    case WebInputEvent::Type::kGestureTap:
      gesture_sequence_in_progress_ = false;
      if (drop_current_tap_ending_event_) {
        drop_current_tap_ending_event_ = false;
        return FilterGestureEventResult::kFiltered;
      }
      break;

    case WebInputEvent::Type::kGestureTapCancel:
      if (drop_current_tap_ending_event_) {
        drop_current_tap_ending_event_ = false;
        return FilterGestureEventResult::kFiltered;
      }
      break;

    case WebInputEvent::Type::kGestureTapDown:
      gesture_sequence_in_progress_ = true;
      allow_cursor_control_ =
          !::features::IsTouchTextEditingRedesignEnabled() ||
          gesture_event->data.tap_down.tap_down_count <= 1;
      // In theory, the num_of_active_touches_ should be > 0 at this point. But
      // crash reports suggest otherwise.
      if (num_of_active_touches_ <= 0)
        SetTouchAction(cc::TouchAction::kAuto);
      active_touch_action_ = allowed_touch_action_;
      DCHECK(!drop_current_tap_ending_event_);
      break;

    case WebInputEvent::Type::kGestureLongPress:
      allow_cursor_control_ = false;
      break;

    case WebInputEvent::Type::kGestureLongTap:
    case WebInputEvent::Type::kGestureTwoFingerTap:
      gesture_sequence_in_progress_ = false;
      break;

    default:
      // Gesture events unrelated to touch actions (panning/zooming) are left
      // alone.
      break;
  }

  return FilterGestureEventResult::kAllowed;
}

void TouchActionFilter::SetTouchAction(cc::TouchAction touch_action) {
  TRACE_EVENT1("input", "TouchActionFilter::SetTouchAction", "action",
               cc::TouchActionToString(touch_action));
  allowed_touch_action_ = touch_action;
  active_touch_action_ = allowed_touch_action_;
  compositor_allowed_touch_action_ = touch_action;
}

FilterGestureEventResult TouchActionFilter::FilterPinchEventAndResetState() {
  if (drop_pinch_events_) {
    drop_pinch_events_ = false;
    return FilterGestureEventResult::kFiltered;
  }
  return FilterGestureEventResult::kAllowed;
}

FilterGestureEventResult TouchActionFilter::FilterScrollEventAndResetState() {
  if (drop_scroll_events_) {
    drop_scroll_events_ = false;
    return FilterGestureEventResult::kFiltered;
  }
  return FilterGestureEventResult::kAllowed;
}

void TouchActionFilter::ForceResetTouchActionForTest() {
  allowed_touch_action_.reset();
  active_touch_action_.reset();
}

void TouchActionFilter::OnSetTouchAction(cc::TouchAction touch_action) {
  TRACE_EVENT2("input", "TouchActionFilter::OnSetTouchAction", "action",
               cc::TouchActionToString(touch_action), "allowed",
               (allowed_touch_action_.has_value()
                    ? cc::TouchActionToString(allowed_touch_action_.value())
                    : "n/a"));
  // TODO(crbug.com/40579429): add a DCHECK for
  // |has_touch_event_handler_|.
  // For multiple fingers, we take the intersection of the touch actions for
  // all fingers that have gone down during this action.  In the majority of
  // real-world scenarios the touch action for all fingers will be the same.
  // This is left as implementation-defined in the pointer events
  // specification because of the relationship to gestures (which are off
  // limits for the spec).  I believe the following are desirable properties
  // of this choice:
  // 1. Not sensitive to finger touch order.  Behavior of putting two fingers
  //    down "at once" will be deterministic.
  // 2. Only subtractive - eg. can't trigger scrolling on a element that
  //    otherwise has scrolling disabling by the addition of a finger.
  allowed_touch_action_ =
      allowed_touch_action_.value_or(cc::TouchAction::kAuto) & touch_action;

  // When user enabled force enable zoom, we should always allow pinch-zoom
  // except for touch-action:none.
  if (force_enable_zoom_ && allowed_touch_action_ != cc::TouchAction::kNone) {
    allowed_touch_action_ =
        allowed_touch_action_.value() | cc::TouchAction::kPinchZoom;
  }
  active_touch_action_ = allowed_touch_action_;
  has_deferred_events_ = false;
}

void TouchActionFilter::IncreaseActiveTouches() {
  TRACE_EVENT1("input", "TouchActionFilter::IncreaseActiveTouches", "num",
               num_of_active_touches_);
  num_of_active_touches_++;
}

void TouchActionFilter::DecreaseActiveTouches() {
  TRACE_EVENT1("input", "TouchActionFilter::DecreaseActiveTouches", "num",
               num_of_active_touches_);
  num_of_active_touches_--;
}

void TouchActionFilter::ReportAndResetTouchAction() {
  if (num_of_active_touches_ <= 0) {
    ResetTouchAction();
    allow_cursor_control_ = true;
  }
}

void TouchActionFilter::ResetTouchAction() {
  TRACE_EVENT0("input", "TouchActionFilter::ResetTouchAction");
  // Note that resetting the action mid-sequence is tolerated. Gestures that had
  // their begin event(s) suppressed will be suppressed until the next sequence.
  if (has_touch_event_handler_) {
    allowed_touch_action_.reset();
    compositor_allowed_touch_action_ = cc::TouchAction::kAuto;
  } else {
    // Lack of a touch handler indicates that the page either has no
    // touch-action modifiers or that all its touch-action modifiers are auto.
    // Resetting the touch-action here allows forwarding of subsequent gestures
    // even if the underlying touches never reach the router.
    SetTouchAction(cc::TouchAction::kAuto);
  }
}

void TouchActionFilter::OnSetCompositorAllowedTouchAction(
    cc::TouchAction allowed_touch_action) {
  TRACE_EVENT2("input", "TouchActionFilter::OnSetCompositorAllowedTouchAction",
               "action", cc::TouchActionToString(allowed_touch_action),
               "current", cc::TouchActionToString(allowed_touch_action));
  // We use '&' here to account for the multiple-finger case, which is the same
  // as OnSetTouchAction.
  compositor_allowed_touch_action_ =
      compositor_allowed_touch_action_ & allowed_touch_action;
}

bool TouchActionFilter::ShouldSuppressScrolling(
    const blink::WebGestureEvent& gesture_event,
    cc::TouchAction touch_action,
    bool is_active_touch_action) {
  DCHECK(gesture_event.GetType() == WebInputEvent::Type::kGestureScrollBegin);
  // If kInternalPanXScrolls is true, kPanX must be true;
  DCHECK((touch_action & cc::TouchAction::kInternalPanXScrolls) ==
             cc::TouchAction::kNone ||
         (touch_action & cc::TouchAction::kPanX) != cc::TouchAction::kNone);

  if (gesture_event.data.scroll_begin.pointer_count >= 2) {
    // Any GestureScrollBegin with more than one fingers is like a pinch-zoom
    // for touch-actions, see crbug.com/632525. Therefore, we switch to
    // blocked-manipulation mode iff pinch-zoom is disallowed.
    return (touch_action & cc::TouchAction::kPinchZoom) ==
           cc::TouchAction::kNone;
  }

  const float& deltaXHint = gesture_event.data.scroll_begin.delta_x_hint;
  const float& deltaYHint = gesture_event.data.scroll_begin.delta_y_hint;

  if (deltaXHint == 0.0 && deltaYHint == 0.0)
    return false;

  const float absDeltaXHint = fabs(deltaXHint);
  const float absDeltaYHint = fabs(deltaYHint);

  // We need to wait for main-thread touch action to see if touch region is
  // writable for stylus handwriting, and accumulate scroll events until then.
  if ((gesture_event.primary_pointer_type ==
           blink::WebPointerProperties::PointerType::kPen ||
       gesture_event.primary_pointer_type ==
           blink::WebPointerProperties::PointerType::kEraser) &&
      !is_active_touch_action &&
      (touch_action & cc::TouchAction::kInternalNotWritable) !=
          cc::TouchAction::kInternalNotWritable)
    return true;

  cc::TouchAction minimal_conforming_touch_action = cc::TouchAction::kNone;
  if (absDeltaXHint > absDeltaYHint) {
    // If we're performing a horizontal gesture over a region that could
    // potentially activate cursor control, we need to wait for the real
    // main-thread touch action before making a decision since we'll need to set
    // the cursor control bit correctly.
    if (!is_active_touch_action &&
        (touch_action & cc::TouchAction::kInternalPanXScrolls) !=
            cc::TouchAction::kInternalPanXScrolls)
      return true;

    if (deltaXHint > 0)
      minimal_conforming_touch_action |= cc::TouchAction::kPanLeft;
    else if (deltaXHint < 0)
      minimal_conforming_touch_action |= cc::TouchAction::kPanRight;
  } else {
    if (deltaYHint > 0)
      minimal_conforming_touch_action |= cc::TouchAction::kPanUp;
    else if (deltaYHint < 0)
      minimal_conforming_touch_action |= cc::TouchAction::kPanDown;
  }
  DCHECK(minimal_conforming_touch_action != cc::TouchAction::kNone);

  return (touch_action & minimal_conforming_touch_action) ==
         cc::TouchAction::kNone;
}

void TouchActionFilter::OnHasTouchEventHandlers(bool has_handlers) {
  TRACE_EVENT1("input", "TouchActionFilter::OnHasTouchEventHandlers",
               "has handlers", has_handlers);
  // The has_touch_event_handler_ is default to false which is why we have the
  // "&&" condition here, to ensure that touch actions will be set if there is
  // no touch event consumers.
  if (has_handlers && has_touch_event_handler_ == has_handlers)
    return;
  has_touch_event_handler_ = has_handlers;
  // We have set the associated touch action if the touch start already happened
  // or there is a gesture in progress. In these cases, we should not reset the
  // associated touch action.
  if (!gesture_sequence_in_progress_ && num_of_active_touches_ <= 0) {
    ResetTouchAction();
    if (has_touch_event_handler_) {
      active_touch_action_.reset();
    }
  }
}

}  // namespace input
