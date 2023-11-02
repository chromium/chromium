// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_UI_DISPLAY_SETTINGS_SCREEN_POWER_CONTROLLER_AURA_H_
#define CHROMECAST_UI_DISPLAY_SETTINGS_SCREEN_POWER_CONTROLLER_AURA_H_

#include "base/memory/weak_ptr.h"
#include "chromecast/ui/display_settings/screen_power_controller.h"

namespace chromecast {

// This class implements the ScreenPowerController with AURA enabled. The class
// doesn't depend on AURA but it will only be available if |use_aura| build flag
// is true. The class wraps the logic of the following transition graph:
//
//          SetScreenOff() && !allow_screen_off
//     On <==================================> Brightness Off
//     /\             SetScreenOn()                 /\
//     ||                                           ||
//     ||                                           ||
//     ||                          allow_screen_off || !allow_screen_off
//     ||                                           ||
//     ||                                           ||
//     ||   SetScreenOff() && allow_screen_off      \/
//     ++=====================================> Power Off
//                     SetScreenOn()
//
//                        Screen Stage Transitions
class ScreenPowerControllerAura : public ScreenPowerController {
 public:
  explicit ScreenPowerControllerAura(ScreenPowerController::Delegate* delegate);
  ~ScreenPowerControllerAura() override;

  void SetScreenOn() override;
  void SetScreenOff() override;
  void SetAllowScreenPowerOff(bool allow_power_off) override;
  bool IsScreenOn() const override;

 private:
  enum PendingTask {
    kNone = 0,
    kOn,
    kBrightnessOff,
    kPowerOff,
  };
  void TriggerPendingTask();
  void SetScreenBrightnessOn(bool brightness_on);
  void SetScreenPowerOn();
  void SetScreenPowerOff();
  void OnScreenPoweredOn(bool succeeded);
  void OnScreenPoweredOff(bool succeeded);
  void OnDisplayOnTimeoutCompleted();
  void OnDisplayOffTimeoutCompleted();

  bool screen_on_;
  bool screen_power_on_;
  bool allow_screen_power_off_;
  PendingTask pending_task_;
  ScreenPowerController::Delegate* delegate_;

  base::WeakPtrFactory<ScreenPowerControllerAura> weak_factory_;
};

}  // namespace chromecast

#endif  // CHROMECAST_UI_DISPLAY_SETTINGS_SCREEN_POWER_CONTROLLER_AURA_H_
