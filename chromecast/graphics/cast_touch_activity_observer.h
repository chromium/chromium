// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_TOUCH_ACTIVITY_OBSERVER_H_
#define CHROMECAST_GRAPHICS_CAST_TOUCH_ACTIVITY_OBSERVER_H_

namespace chromecast {

class CastTouchActivityObserver {
 public:
  virtual ~CastTouchActivityObserver() = default;

  // Invoked when the window manager has touch input disabled.
  virtual void OnTouchEventsDisabled(bool disabled) = 0;

  // Invoked when input is disabled and an input event is received.
  // Can be used by the observer to turn touch input back on.
  virtual void OnTouchActivity() = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_CAST_TOUCH_ACTIVITY_OBSERVER_H_
