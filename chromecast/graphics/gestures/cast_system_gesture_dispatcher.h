// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_GESTURES_CAST_SYSTEM_GESTURE_DISPATCHER_H_
#define CHROMECAST_GRAPHICS_GESTURES_CAST_SYSTEM_GESTURE_DISPATCHER_H_

#include <queue>

#include "base/containers/flat_set.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chromecast/graphics/gestures/cast_gesture_handler.h"

namespace chromecast {

// A CastGestureHandler that fans system gesture events out to multiple
// consumers.
class CastSystemGestureDispatcher : public CastGestureHandler {
 public:
  CastSystemGestureDispatcher();
  explicit CastSystemGestureDispatcher(const base::TickClock* tick_clock);

  ~CastSystemGestureDispatcher() override;

  // Register a new handler for gesture events, including side swipe and tap.
  void AddGestureHandler(CastGestureHandler* handler);

  // Remove the registration of a gesture handler.
  void RemoveGestureHandler(CastGestureHandler* handler);

  // Implementation of CastGestureHandler methods which fan out to our gesture
  // handlers.
  Priority GetPriority() override;
  bool CanHandleSwipe(CastSideSwipeOrigin swipe_origin) override;
  void HandleSideSwipe(CastSideSwipeEvent event,
                       CastSideSwipeOrigin swipe_origin,
                       const gfx::Point& touch_location) override;
  void HandleTapDownGesture(const gfx::Point& touch_location) override;
  void HandleTapGesture(const gfx::Point& touch_location) override;

 private:
  // Logs a completed gesture event.
  struct GestureEvent {
    base::TimeTicks event_time;
    CastSideSwipeOrigin origin;
  };

  std::queue<GestureEvent> recent_events_;
  bool send_gestures_to_root_;
  const base::TickClock* const tick_clock_;
  base::flat_set<CastGestureHandler*> gesture_handlers_;
};
}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_GESTURES_CAST_SYSTEM_GESTURE_DISPATCHER_H_
