// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/device_actions.h"

#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/network/network_state_handler.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

using chromeos::NetworkHandler;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

DeviceActions::DeviceActions() {}

DeviceActions::~DeviceActions() = default;

void DeviceActions::SetWifiEnabled(bool enabled) {
  NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), enabled,
      chromeos::network_handler::ErrorCallback());
}

void DeviceActions::SetBluetoothEnabled(bool enabled) {
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  DCHECK(profile);
  // Simply toggle the user pref, which is being observed by ash's bluetooth
  // power controller.
  profile->GetPrefs()->SetBoolean(ash::prefs::kUserBluetoothAdapterEnabled,
                                  enabled);
}

void HandleScreenBrightnessCallback(
    DeviceActions::GetScreenBrightnessLevelCallback callback,
    base::Optional<double> level) {
  if (level.has_value()) {
    std::move(callback).Run(true, level.value() / 100.0);
  } else {
    std::move(callback).Run(false, 0.0);
  }
}

void DeviceActions::GetScreenBrightnessLevel(
    DeviceActions::GetScreenBrightnessLevelCallback callback) {
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->GetScreenBrightnessPercent(
          base::BindOnce(&HandleScreenBrightnessCallback, std::move(callback)));
}

void DeviceActions::SetScreenBrightnessLevel(double level, bool gradual) {
  power_manager::SetBacklightBrightnessRequest request;
  request.set_percent(level * 100);
  request.set_transition(
      gradual
          ? power_manager::SetBacklightBrightnessRequest_Transition_GRADUAL
          : power_manager::SetBacklightBrightnessRequest_Transition_INSTANT);
  request.set_cause(
      power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST);
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->SetScreenBrightness(request);
}

void DeviceActions::SetNightLightEnabled(bool enabled) {
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  DCHECK(profile);
  // Simply toggle the user pref, which is being observed by ash's night
  // light controller.
  profile->GetPrefs()->SetBoolean(ash::prefs::kNightLightEnabled, enabled);
}
