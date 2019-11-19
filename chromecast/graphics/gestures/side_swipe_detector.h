// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_GESTURES_SIDE_SWIPE_DETECTOR_H_
#define CHROMECAST_GRAPHICS_GESTURES_SIDE_SWIPE_DETECTOR_H_

#include <deque>

#include "base/timer/elapsed_timer.h"
#include "chromecast/graphics/gestures/cast_gesture_handler.h"
#include "ui/events/event_rewriter.h"

namespace aura {
class Window;
}  // namespace aura

namespace chromecast {

// An event rewriter for detecting system-wide gestures performed on the margins
// of the root window.
// Recognizes swipe gestures that originate from the top, left, bottom, and
// right of the root window.  Stashes copies of touch events that occur during
// the side swipe, and replays them if the finger releases before leaving the
// margin area.
class SideSwipeDetector : public ui::EventRewriter {
 public:
  SideSwipeDetector(CastGestureHandler* gesture_handler,
                    aura::Window* root_window);

  ~SideSwipeDetector() override;

  CastSideSwipeOrigin GetDragPosition(const gfx::Point& point,
                                      const gfx::Rect& screen_bounds) const;

  // Overridden from ui::EventRewriter
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

 private:
  void StashEvent(const ui::TouchEvent& event);

  const int gesture_start_width_;
  const int gesture_start_height_;
  const int bottom_gesture_start_height_;

  CastGestureHandler* gesture_handler_;
  aura::Window* root_window_;
  CastSideSwipeOrigin current_swipe_;
  ui::PointerId current_pointer_id_;
  base::ElapsedTimer current_swipe_time_;

  std::deque<ui::TouchEvent> stashed_events_;

  DISALLOW_COPY_AND_ASSIGN(SideSwipeDetector);
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_GESTURES_SIDE_SWIPE_DETECTOR_H_
