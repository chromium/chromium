// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_UI_DISPLAY_SETTINGS_SCREEN_POWER_CONTROLLER_DEFAULT_H_
#define CHROMECAST_UI_DISPLAY_SETTINGS_SCREEN_POWER_CONTROLLER_DEFAULT_H_

#include "base/memory/weak_ptr.h"
#include "chromecast/ui/display_settings/screen_power_controller.h"

namespace chromecast {

// This class implements the ScreenPowerController's default behavior. It never
// powers off the screen so most of the methods will do nothing other than
// logging and invoke delegate's methods.
class ScreenPowerControllerDefault : public ScreenPowerController {
 public:
  explicit ScreenPowerControllerDefault(
      ScreenPowerController::Delegate* delegate);
  ~ScreenPowerControllerDefault() override;

  void SetScreenOn() override;
  void SetScreenOff() override;
  void SetAllowScreenPowerOff(bool allow_power_off) override;
  bool IsScreenOn() const override;

 private:
  bool screen_on_;
  ScreenPowerController::Delegate* delegate_;
};

}  // namespace chromecast

#endif  // CHROMECAST_UI_DISPLAY_SETTINGS_SCREEN_POWER_CONTROLLER_DEFAULT_H_
