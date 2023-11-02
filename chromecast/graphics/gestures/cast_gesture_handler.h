// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_GESTURES_CAST_GESTURE_HANDLER_H_
#define CHROMECAST_GRAPHICS_GESTURES_CAST_GESTURE_HANDLER_H_

namespace gfx {
class Point;
}  // namespace gfx

namespace chromecast {

enum class CastSideSwipeOrigin { TOP, BOTTOM, LEFT, RIGHT, NONE };

enum class CastSideSwipeEvent {
  // Indicates the beginning of a side-swipe event (finger down).
  BEGIN,
  // Indicates the continuance of a side-swipe event (finger drag).
  CONTINUE,
  // Indicates the end of a side-swipe event (finger up).
  END
};

// Interface for handlers triggered on reception of gestures on the
// root window, including side-swipe and tap.
class CastGestureHandler {
 public:
  // Gesture handler priority. If multiple handlers can process a gesture event,
  // then the highest-priority handler is the one which receives the event.
  enum class Priority {
    // Handler with NONE priority will never receive a gesture event.
    NONE = 0,
    ROOT_UI,
    MAIN_ACTIVITY,
    SETTINGS_UI,
    MAX = SETTINGS_UI,
  };

  CastGestureHandler() = default;

  CastGestureHandler(const CastGestureHandler&) = delete;
  CastGestureHandler& operator=(const CastGestureHandler&) = delete;

  virtual ~CastGestureHandler() = default;

  // Returns the gesture handler's current priority.
  virtual Priority GetPriority() = 0;

  // Return true if this handler can handle swipes from the given origin.
  virtual bool CanHandleSwipe(CastSideSwipeOrigin swipe_origin) = 0;

  // Triggered when a user swipes from an edge on the screen.
  virtual void HandleSideSwipe(CastSideSwipeEvent event,
                               CastSideSwipeOrigin swipe_origin,
                               const gfx::Point& touch_location) = 0;

  // Triggered on the completion of a tap down event, fired when the
  // finger is pressed.
  virtual void HandleTapDownGesture(const gfx::Point& touch_location) = 0;

  // Triggered on the completion of a tap event, fire after a press
  // followed by a release, within the tap timeout window
  virtual void HandleTapGesture(const gfx::Point& touch_location) = 0;
};

}  // namespace chromecast

#endif  //  CHROMECAST_GRAPHICS_GESTURES_CAST_GESTURE_HANDLER_H_
