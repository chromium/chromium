// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/login_ui_pref_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash {

LoginUIPrefController::LoginUIPrefController() {
  PrefService* prefs = g_browser_process->local_state();
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kOwnerPrimaryMouseButtonRight,
      base::BindRepeating(&LoginUIPrefController::UpdatePrimaryMouseButtonRight,
                          weak_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      prefs::kOwnerPrimaryPointingStickButtonRight,
      base::BindRepeating(
          &LoginUIPrefController::UpdatePrimaryPointingStickButtonRight,
          weak_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      prefs::kOwnerTapToClickEnabled,
      base::BindRepeating(&LoginUIPrefController::UpdateTapToClickEnabled,
                          weak_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      ash::prefs::kDeviceGeolocationAllowed,
      base::BindRepeating(&LoginUIPrefController::UpdateGeolocationUsageAllowed,
                          weak_factory_.GetWeakPtr()));

  if (prefs->GetAllPrefStoresInitializationStatus() ==
      PrefService::INITIALIZATION_STATUS_WAITING) {
    prefs->AddPrefInitObserver(
        base::BindOnce(&LoginUIPrefController::InitOwnerPreferences,
                       weak_factory_.GetWeakPtr()));
  } else {
    InitOwnerPreferences(prefs->GetAllPrefStoresInitializationStatus() !=
                         PrefService::INITIALIZATION_STATUS_ERROR);
  }
}

LoginUIPrefController::~LoginUIPrefController() = default;

void LoginUIPrefController::UpdatePrimaryMouseButtonRight() {
  system::InputDeviceSettings::Get()->SetPrimaryButtonRight(
      g_browser_process->local_state()->GetBoolean(
          prefs::kOwnerPrimaryMouseButtonRight));
}

void LoginUIPrefController::UpdatePrimaryPointingStickButtonRight() {
  system::InputDeviceSettings::Get()->SetPointingStickPrimaryButtonRight(
      g_browser_process->local_state()->GetBoolean(
          prefs::kOwnerPrimaryPointingStickButtonRight));
}

void LoginUIPrefController::UpdateTapToClickEnabled() {
  system::InputDeviceSettings::Get()->SetTapToClick(
      g_browser_process->local_state()->GetBoolean(
          prefs::kOwnerTapToClickEnabled));
}

void LoginUIPrefController::UpdateGeolocationUsageAllowed() {
  // Set the log-in screen geolocation access permission to the
  // `SimpleGeolocationProvider` global instance.
  SimpleGeolocationProvider::GetInstance()->SetGeolocationAccessLevel(
      static_cast<GeolocationAccessLevel>(
          g_browser_process->local_state()->GetInteger(
              ash::prefs::kDeviceGeolocationAllowed)));
}

void LoginUIPrefController::InitOwnerPreferences(bool success) {
  if (!success) {
    LOG(ERROR) << "InitOwnerPreferences failed.";
    return;
  }
  UpdatePrimaryMouseButtonRight();
  UpdatePrimaryPointingStickButtonRight();
  UpdateTapToClickEnabled();
  UpdateGeolocationUsageAllowed();
}

}  // namespace ash
