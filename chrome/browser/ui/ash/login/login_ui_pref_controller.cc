// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/login_ui_pref_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/geolocation/system_location_provider.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash {

LoginUIPrefController::LoginUIPrefController(
    PrefService* local_state,
    bool update_geolocation_usage_allowed)
    : local_state_(CHECK_DEREF(local_state)),
      update_geolocation_usage_allowed_(update_geolocation_usage_allowed) {
  pref_change_registrar_.Init(&local_state_.get());
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
  if (update_geolocation_usage_allowed_) {
    pref_change_registrar_.Add(
        ash::prefs::kDeviceGeolocationAllowed,
        base::BindRepeating(
            &LoginUIPrefController::UpdateGeolocationUsageAllowed,
            weak_factory_.GetWeakPtr()));
  }

  if (local_state_->GetAllPrefStoresInitializationStatus() ==
      PrefService::INITIALIZATION_STATUS_WAITING) {
    local_state_->AddPrefInitObserver(
        base::BindOnce(&LoginUIPrefController::InitOwnerPreferences,
                       weak_factory_.GetWeakPtr()));
  } else {
    InitOwnerPreferences(local_state_->GetAllPrefStoresInitializationStatus() !=
                         PrefService::INITIALIZATION_STATUS_ERROR);
  }
}

LoginUIPrefController::~LoginUIPrefController() = default;

void LoginUIPrefController::UpdatePrimaryMouseButtonRight() {
  system::InputDeviceSettings::Get()->SetPrimaryButtonRight(
      local_state_->GetBoolean(prefs::kOwnerPrimaryMouseButtonRight));
}

void LoginUIPrefController::UpdatePrimaryPointingStickButtonRight() {
  system::InputDeviceSettings::Get()->SetPointingStickPrimaryButtonRight(
      local_state_->GetBoolean(prefs::kOwnerPrimaryPointingStickButtonRight));
}

void LoginUIPrefController::UpdateTapToClickEnabled() {
  system::InputDeviceSettings::Get()->SetTapToClick(
      local_state_->GetBoolean(prefs::kOwnerTapToClickEnabled));
}

void LoginUIPrefController::UpdateGeolocationUsageAllowed() {
  // Set the log-in screen geolocation access permission to the
  // `SystemLocationProvider` global instance.
  SystemLocationProvider::GetInstance()->SetGeolocationAccessLevel(
      static_cast<GeolocationAccessLevel>(
          local_state_->GetInteger(ash::prefs::kDeviceGeolocationAllowed)));
}

void LoginUIPrefController::InitOwnerPreferences(bool success) {
  if (!success) {
    LOG(ERROR) << "InitOwnerPreferences failed.";
    return;
  }
  UpdatePrimaryMouseButtonRight();
  UpdatePrimaryPointingStickButtonRight();
  UpdateTapToClickEnabled();
  if (update_geolocation_usage_allowed_) {
    UpdateGeolocationUsageAllowed();
  }
}

}  // namespace ash
