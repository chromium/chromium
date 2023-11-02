// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/gesture_detector.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/numerics/math_constants.h"
#include "chrome/browser/vr/input_event.h"
#include "chrome/browser/vr/platform_controller.h"

namespace vr {

namespace {

constexpr float kDisplacementScaleFactor = 129.0f;

constexpr int kMaxNumOfExtrapolations = 2;

// Minimum time distance needed to call two timestamps
// not equal.
constexpr float kDelta = 1.0e-7f;

constexpr float kCutoffHz = 10.0f;
constexpr float kRC = 1.0f / (2.0f * base::kPiFloat * kCutoffHz);

// A slop represents a small rectangular region around the first touch point of
// a gesture.
// If the user does not move outside of the slop, no gesture is detected.
// Gestures start to be detected when the user moves outside of the slop.
// Vertical distance from the border to the center of slop.
constexpr float kSlopVertical = 0.165f;

// Horizontal distance from the border to the center of slop.
constexpr float kSlopHorizontal = 0.15f;

// Exceeding pressing the appbutton for longer than this threshold will result
// in a long press.
constexpr base::TimeDelta kLongPressThreshold = base::Milliseconds(900);

struct TouchPoint {
  gfx::Vector2dF position;
  base::TimeTicks timestamp;
};

}  // namespace

GestureDetector::GestureDetector() {
  Reset();
}
GestureDetector::~GestureDetector() = default;

InputEventList GestureDetector::DetectGestures(
    const PlatformController& controller,
    base::TimeTicks current_timestamp) {
  touch_position_changed_ = UpdateCurrentTouchPoint(controller);
  TouchPoint touch_point{.position = controller.GetPositionInTrackpad(),
                         .timestamp = controller.GetLastTouchTimestamp()};
  ExtrapolateTouchPoint(&touch_point, current_timestamp);
  if (touch_position_changed_)
    UpdateOverallVelocity(touch_point);
  is_select_button_pressed_ =
      controller.IsButtonDown(PlatformController::kButtonSelect);
  last_touching_state_ = is_touching_trackpad_;
  is_touching_trackpad_ = controller.IsTouchingTrackpad();

  InputEventList gesture_list;
  DetectMenuButtonGestures(&gesture_list, controller, current_timestamp);
  auto gesture = GetGestureFromTouchInfo(touch_point);

  if (!gesture)
    return gesture_list;

  if (gesture->type() == InputEvent::kScrollEnd)
    Reset();

  if (gesture->type() != InputEvent::kTypeUndefined)
    gesture_list.push_back(std::move(gesture));

  return gesture_list;
}

void GestureDetector::DetectMenuButtonGestures(
    InputEventList* event_list,
    const PlatformController& controller,
    base::TimeTicks current_timestamp) {
  std::unique_ptr<InputEvent> event;
  if (controller.ButtonDownHappened(PlatformController::kButtonMenu)) {
    menu_button_down_timestamp_ = current_timestamp;
    menu_button_long_pressed_ = false;
  }
  if (controller.ButtonUpHappened(PlatformController::kButtonMenu)) {
    event = std::make_unique<InputEvent>(
        menu_button_long_pressed_ ? InputEvent::kMenuButtonLongPressEnd
                                  : InputEvent::kMenuButtonClicked);
  }
  if (!menu_button_long_pressed_ &&
      controller.IsButtonDown(PlatformController::kButtonMenu) &&
      current_timestamp - menu_button_down_timestamp_ > kLongPressThreshold) {
    menu_button_long_pressed_ = true;
    event = std::make_unique<InputEvent>(InputEvent::kMenuButtonLongPressStart);
  }
  if (event) {
    event->set_time_stamp(current_timestamp);
    event_list->push_back(std::move(event));
  }
}

std::unique_ptr<InputEvent> GestureDetector::GetGestureFromTouchInfo(
    const TouchPoint& touch_point) {
  std::unique_ptr<InputEvent> gesture;

  switch (state_->label) {
    // User has not put finger on touch pad.
    case WAITING:
      gesture = HandleWaitingState(touch_point);
      break;
    // User has not started a gesture (by moving out of slop).
    case TOUCHING:
      gesture = HandleDetectingState(touch_point);
      break;
    // User is scrolling on touchpad
    case SCROLLING:
      gesture = HandleScrollingState(touch_point);
      break;
    // The user has finished scrolling, but we'll hallucinate a few points
    // before really finishing.
    case POST_SCROLL:
      gesture = HandlePostScrollingState(touch_point);
      break;
    default:
      NOTREACHED();
      break;
  }

  if (gesture) {
    // |touch_point.timestamp| will only update when the touchpad is actually
    // being touched. This causes issues with the three extra scroll updates we
    // send after the touchpad is no longer touched (one in the SCROLLING state,
    // two in the POST_SCROLL state). The timestamps for scroll events need to
    // be monotonically increasing, but since we generate two scroll updates
    // per frame, with the second being slightly in the future from the "real"
    // timestamp, if the timestamp we provide does not increase each frame,
    // we end up sending the timestamp sequence A -> A + 1 -> A, which hits
    // a DCHECK because the second A timestamp is received after the (larger)
    // A + 1.
    // We also can't just pass |last_timestamp_| because it appears to be offset
    // from the touchpad timestamp by ~10ms. Instead, keep track of the
    // |last_timestamp_| value we had the last time the touchpad as actually
    // touched, and add the difference between the current |last_timestamp_| and
    // that to the |last_touch_timestamp_| to get a good approximation of the
    // current timestamp in the touch timestamp's time base.
    if (should_fake_timestamp_) {
      auto fake_timestamp =
          last_touch_timestamp_ +
          (last_timestamp_ - last_touch_timestamp_local_timebase_);
      gesture->set_time_stamp(fake_timestamp);
    } else {
      gesture->set_time_stamp(touch_point.timestamp);
    }
  }

  return gesture;
}

std::unique_ptr<InputEvent> GestureDetector::HandleWaitingState(
    const TouchPoint& touch_point) {
  // User puts finger on touch pad (or when the touch down for current gesture
  // is missed, initiate gesture from current touch point).
  if (is_touching_trackpad_) {
    // update initial touchpoint
    state_->initial_touch_point = touch_point;
    // update current touchpoint
    state_->cur_touch_point = touch_point;
    state_->label = TOUCHING;

    return std::make_unique<InputEvent>(InputEvent::kFlingCancel);
  }
  return nullptr;
}

std::unique_ptr<InputEvent> GestureDetector::HandleDetectingState(
    const TouchPoint& touch_point) {
  // User lifts up finger from touch pad.
  if (!is_touching_trackpad_) {
    Reset();
    return nullptr;
  }

  // Touch position is changed, the touch point moves outside of slop,
  // and the Controller's button is not down.
  if (touch_position_changed_ && is_touching_trackpad_ &&
      !InSlop(touch_point.position) && !is_select_button_pressed_) {
    state_->label = SCROLLING;
    auto gesture = std::make_unique<InputEvent>(InputEvent::kScrollBegin);
    UpdateGestureParameters(touch_point);
    UpdateGestureWithScrollDelta(gesture.get());
    return gesture;
  }
  return nullptr;
}

std::unique_ptr<InputEvent> GestureDetector::HandleScrollingState(
    const TouchPoint& touch_point) {
  if (is_select_button_pressed_) {
    UpdateGestureParameters(touch_point);
    return std::make_unique<InputEvent>(InputEvent::kScrollEnd);
  }
  if (!is_touching_trackpad_)
    state_->label = POST_SCROLL;
  if (touch_position_changed_) {
    auto gesture = std::make_unique<InputEvent>(InputEvent::kScrollUpdate);
    UpdateGestureParameters(touch_point);
    UpdateGestureWithScrollDelta(gesture.get());
    return gesture;
  }
  return nullptr;
}

std::unique_ptr<InputEvent> GestureDetector::HandlePostScrollingState(
    const TouchPoint& touch_point) {
  if (extrapolated_touch_ == 0 || is_select_button_pressed_) {
    UpdateGestureParameters(touch_point);
    return std::make_unique<InputEvent>(InputEvent::kScrollEnd);
  } else {
    auto gesture = std::make_unique<InputEvent>(InputEvent::kScrollUpdate);
    UpdateGestureParameters(touch_point);
    UpdateGestureWithScrollDelta(gesture.get());
    return gesture;
  }
}

void GestureDetector::UpdateGestureWithScrollDelta(InputEvent* gesture) {
  gesture->scroll_data.delta_x =
      state_->displacement.x() * kDisplacementScaleFactor;
  gesture->scroll_data.delta_y =
      state_->displacement.y() * kDisplacementScaleFactor;
}

bool GestureDetector::UpdateCurrentTouchPoint(
    const PlatformController& controller) {
  if (controller.IsTouchingTrackpad() || last_touching_state_) {
    // Update the touch point when the touch position has changed.
    if (state_->cur_touch_point.position !=
        controller.GetPositionInTrackpad()) {
      state_->prev_touch_point = state_->cur_touch_point;
      state_->cur_touch_point = {
          .position = controller.GetPositionInTrackpad(),
          .timestamp = controller.GetLastTouchTimestamp()};
      return true;
    }
  }
  return false;
}

void GestureDetector::ExtrapolateTouchPoint(TouchPoint* touch_point,
                                            base::TimeTicks current_timestamp) {
  should_fake_timestamp_ = false;
  const bool effectively_scrolling =
      state_->label == SCROLLING || state_->label == POST_SCROLL;
  if (effectively_scrolling && extrapolated_touch_ < kMaxNumOfExtrapolations &&
      (touch_point->timestamp == last_touch_timestamp_ ||
       touch_point->position == state_->prev_touch_point.position)) {
    extrapolated_touch_++;
    touch_position_changed_ = true;
    float duration = (current_timestamp - last_timestamp_).InSecondsF();
    touch_point->position.set_x(state_->cur_touch_point.position.x() +
                                state_->overall_velocity.x() * duration);
    touch_point->position.set_y(state_->cur_touch_point.position.y() +
                                state_->overall_velocity.y() * duration);
    if (last_touch_timestamp_ == touch_point->timestamp) {
      should_fake_timestamp_ = true;
    }
  } else {
    if (extrapolated_touch_ == kMaxNumOfExtrapolations) {
      state_->overall_velocity = {0, 0};
    }
    extrapolated_touch_ = 0;
  }
  if (touch_point->timestamp > last_touch_timestamp_) {
    last_touch_timestamp_local_timebase_ = current_timestamp;
  }
  last_touch_timestamp_ = touch_point->timestamp;
  last_timestamp_ = current_timestamp;
}

void GestureDetector::UpdateOverallVelocity(const TouchPoint& touch_point) {
  float duration =
      (touch_point.timestamp - state_->prev_touch_point.timestamp).InSecondsF();
  // If the timestamp does not change, do not update velocity.
  if (duration < kDelta)
    return;

  const gfx::Vector2dF& displacement =
      touch_point.position - state_->prev_touch_point.position;

  const gfx::Vector2dF& velocity = ScaleVector2d(displacement, (1 / duration));

  float weight = duration / (kRC + duration);

  state_->overall_velocity =
      ScaleVector2d(state_->overall_velocity, (1 - weight)) +
      ScaleVector2d(velocity, weight);
}

void GestureDetector::UpdateGestureParameters(const TouchPoint& touch_point) {
  state_->displacement =
      touch_point.position - state_->prev_touch_point.position;
}

bool GestureDetector::InSlop(const gfx::PointF touch_position) const {
  return (std::abs(touch_position.x() -
                   state_->initial_touch_point.position.x()) <
          kSlopHorizontal) &&
         (std::abs(touch_position.y() -
                   state_->initial_touch_point.position.y()) < kSlopVertical);
}

void GestureDetector::Reset() {
  state_ = std::make_unique<GestureDetectorState>();
}

}  // namespace vr
