// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_CONTENT_GESTURE_HANDLER_H_
#define CHROMECAST_BROWSER_CAST_CONTENT_GESTURE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "chromecast/browser/gesture_router.h"
#include "chromecast/graphics/gestures/cast_gesture_handler.h"

namespace chromecast {

// Receives root window level gestures, interprets them, and hands them to the
// CastContentWindow::Delegate.
class CastContentGestureHandler : public CastGestureHandler {
 public:
  explicit CastContentGestureHandler(GestureRouter* delegate);
  CastContentGestureHandler(GestureRouter* delegate,
                            bool enable_top_drag_gesture);
  ~CastContentGestureHandler() override;

  // CastGestureHandler implementation:
  Priority GetPriority() override;
  bool CanHandleSwipe(CastSideSwipeOrigin swipe_origin) override;
  void HandleSideSwipe(CastSideSwipeEvent event,
                       CastSideSwipeOrigin swipe_origin,
                       const gfx::Point& touch_location) override;
  void HandleTapDownGesture(const gfx::Point& touch_location) override;
  void HandleTapGesture(const gfx::Point& touch_location) override;

  void SetPriority(Priority priority);

 private:
  GestureType GestureForSwipeOrigin(CastSideSwipeOrigin swipe_origin);

  Priority priority_;

  const bool enable_top_drag_gesture_;

  // Number of pixels past swipe origin to consider as a back gesture.
  const int back_horizontal_threshold_;
  GestureRouter* const delegate_;
  base::ElapsedTimer current_swipe_time_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_CONTENT_GESTURE_HANDLER_H_
