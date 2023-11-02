// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_GESTURE_DETECTOR_H_
#define CHROME_BROWSER_VR_GESTURE_DETECTOR_H_

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/vr/vr_export.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace vr {

class InputEvent;
class PlatformController;

using InputEventList = std::vector<std::unique_ptr<InputEvent>>;

class VR_EXPORT GestureDetector {
 public:
  GestureDetector();

  GestureDetector(const GestureDetector&) = delete;
  GestureDetector& operator=(const GestureDetector&) = delete;

  virtual ~GestureDetector();

  InputEventList DetectGestures(const PlatformController& controller,
                                base::TimeTicks current_timestamp);

 private:
  enum GestureDetectorStateLabel {
    WAITING,     // waiting for user to touch down
    TOUCHING,    // touching the touch pad but not scrolling
    SCROLLING,   // scrolling on the touch pad
    POST_SCROLL  // scroll has finished and we are hallucinating events
  };

  struct TouchPoint {
    gfx::PointF position;
    base::TimeTicks timestamp;
  };

  struct GestureDetectorState {
    GestureDetectorStateLabel label = WAITING;
    TouchPoint prev_touch_point;
    TouchPoint cur_touch_point;
    TouchPoint initial_touch_point;
    gfx::Vector2dF overall_velocity;

    // Displacement of the touch point from the previews to the current touch
    gfx::Vector2dF displacement;
  };

  void DetectMenuButtonGestures(InputEventList* event_list,
                                const PlatformController& controller,
                                base::TimeTicks current_timestamp);

  std::unique_ptr<InputEvent> GetGestureFromTouchInfo(
      const TouchPoint& touch_point);

  std::unique_ptr<InputEvent> HandleWaitingState(const TouchPoint& touch_point);
  std::unique_ptr<InputEvent> HandleDetectingState(
      const TouchPoint& touch_point);
  std::unique_ptr<InputEvent> HandleScrollingState(
      const TouchPoint& touch_point);
  std::unique_ptr<InputEvent> HandlePostScrollingState(
      const TouchPoint& touch_point);

  void UpdateGestureWithScrollDelta(InputEvent* gesture);

  // If the user is touching the touch pad and the touch point is different from
  // before, update the touch point and return true. Otherwise, return false.
  bool UpdateCurrentTouchPoint(const PlatformController& controller);

  void ExtrapolateTouchPoint(TouchPoint* touch_point,
                             base::TimeTicks current_timestamp);

  void UpdateOverallVelocity(const TouchPoint& touch_info);

  void UpdateGestureParameters(const TouchPoint& touch_info);

  bool InSlop(const gfx::PointF touch_position) const;

  void Reset();

  std::unique_ptr<GestureDetectorState> state_;

  bool is_select_button_pressed_ = false;
  bool is_touching_trackpad_ = false;

  // Number of consecutively extrapolated touch points
  int extrapolated_touch_ = 0;

  base::TimeTicks last_touch_timestamp_;
  base::TimeTicks last_timestamp_;
  bool last_touching_state_ = false;

  bool should_fake_timestamp_ = false;
  base::TimeTicks last_touch_timestamp_local_timebase_;

  bool touch_position_changed_;

  base::TimeTicks menu_button_down_timestamp_;
  bool menu_button_long_pressed_ = false;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_GESTURE_DETECTOR_H_
