// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_UI_DISPLAY_SETTINGS_SCREEN_POWER_CONTROLLER_H_
#define CHROMECAST_UI_DISPLAY_SETTINGS_SCREEN_POWER_CONTROLLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/time/time.h"

namespace chromecast {

// This is an interface that controls the screen power. There are two stages
// when the screen is off: brightness off (zero) and power off. Whether the
// screen backlight is set to zero or the screen is powered off is abstracted in
// this class. Higher-level services generally do not care which mode it's in.
// The interface doesn't perform actual actions but invokes its delegate to turn
// on/off the screen, which is easy to isolate with mock classes and write
// unittest.
class ScreenPowerController {
 public:
  // The actual class that controls the brightness and the power of the
  // screen.
  class Delegate {
   public:
    using PowerToggleCallback = base::OnceCallback<void(bool)>;

    virtual ~Delegate() = default;
    // Turns the screen power on and calls the |callback| with a bool that
    // indicates if the action is successful.
    virtual void SetScreenPowerOn(PowerToggleCallback callback) = 0;
    // Turns the screen power off and calls the |callback| with a bool that
    // indicates if the action is successful.
    virtual void SetScreenPowerOff(PowerToggleCallback callback) = 0;
    // Turns the screen brightness on/off according to |brightness_on| with an
    // animation of |duration|.
    virtual void SetScreenBrightnessOn(bool brightness_on,
                                       base::TimeDelta duration) = 0;
  };

  // The factory method that creates the derived implementation class.
  static std::unique_ptr<ScreenPowerController> Create(Delegate* delegate);

  virtual ~ScreenPowerController() = default;

  // Turns on the screen.
  virtual void SetScreenOn() = 0;
  // Turns off the screen and lets the derived class decide which stage should
  // the screen transfer to.
  virtual void SetScreenOff() = 0;
  // Sets whether the screen is allow to power off.
  virtual void SetAllowScreenPowerOff(bool allow_power_off) = 0;
  // Returns the state whether the screen is on/off.
  virtual bool IsScreenOn() const = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_UI_DISPLAY_SETTINGS_SCREEN_POWER_CONTROLLER_H_
