// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_UI_PREF_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_UI_PREF_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"

namespace ash {

class LoginUIPrefController {
 public:
  // Applies effects of login-screen prefs, e.g., mouse and touchpad settings
  // that are coming from the owner user and/or device policies.
  LoginUIPrefController();
  LoginUIPrefController(const LoginUIPrefController&) = delete;
  LoginUIPrefController& operator=(const LoginUIPrefController&) = delete;
  ~LoginUIPrefController();

 private:
  // Apply the owner preferences after local_state prefService start.
  void InitOwnerPreferences(bool success);

  // Apply "owner.mouse.primary_right" preference on the login screen.
  void UpdatePrimaryMouseButtonRight();

  // Apply "owner.pointing_stick.primary_right" preference on the login screen.
  void UpdatePrimaryPointingStickButtonRight();

  // Apply "owner.touchpad.enable_tap_to_click" preference on the login screen.
  void UpdateTapToClickEnabled();

  // Apply "ash.device.geolocation_allowed" preference on the login screen.
  void UpdateGeolocationUsageAllowed();

  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<LoginUIPrefController> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_UI_PREF_CONTROLLER_H_
