// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/proximity_auth_profile_pref_manager.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_pref_names.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace proximity_auth {

ProximityAuthProfilePrefManager::ProximityAuthProfilePrefManager(
    PrefService* pref_service,
    ash::multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : pref_service_(pref_service),
      multidevice_setup_client_(multidevice_setup_client) {}

ProximityAuthProfilePrefManager::~ProximityAuthProfilePrefManager() = default;
// static
void ProximityAuthProfilePrefManager::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kEasyUnlockEnabledStateSet, false);
  registry->RegisterInt64Pref(
      prefs::kProximityAuthLastPromotionCheckTimestampMs, 0L);
  registry->RegisterIntegerPref(prefs::kProximityAuthPromotionShownCount, 0);
  registry->RegisterDictionaryPref(prefs::kProximityAuthRemoteBleDevices);
}

bool ProximityAuthProfilePrefManager::IsEasyUnlockAllowed() const {
  return pref_service_->GetBoolean(
      ash::multidevice_setup::kSmartLockAllowedPrefName);
}

void ProximityAuthProfilePrefManager::SetIsEasyUnlockEnabled(
    bool is_easy_unlock_enabled) const {
  pref_service_->SetBoolean(
      ash::multidevice_setup::kSmartLockEnabledDeprecatedPrefName,
      is_easy_unlock_enabled);
}

bool ProximityAuthProfilePrefManager::IsEasyUnlockEnabled() const {
  // Note: if GetFeatureState() is called in the first few hundred milliseconds
  // of user session startup, it can incorrectly return a feature-default state
  // of kProhibitedByPolicy. See https://crbug.com/1154766 for more.
  return multidevice_setup_client_->GetFeatureState(
             ash::multidevice_setup::mojom::Feature::kSmartLock) ==
         ash::multidevice_setup::mojom::FeatureState::kEnabledByUser;
}

void ProximityAuthProfilePrefManager::SetEasyUnlockEnabledStateSet() const {
  return pref_service_->SetBoolean(prefs::kEasyUnlockEnabledStateSet, true);
}

bool ProximityAuthProfilePrefManager::IsEasyUnlockEnabledStateSet() const {
  return pref_service_->GetBoolean(prefs::kEasyUnlockEnabledStateSet);
}

void ProximityAuthProfilePrefManager::SetLastPromotionCheckTimestampMs(
    int64_t timestamp_ms) {
  pref_service_->SetInt64(prefs::kProximityAuthLastPromotionCheckTimestampMs,
                          timestamp_ms);
}

int64_t ProximityAuthProfilePrefManager::GetLastPromotionCheckTimestampMs()
    const {
  return pref_service_->GetInt64(
      prefs::kProximityAuthLastPromotionCheckTimestampMs);
}

void ProximityAuthProfilePrefManager::SetPromotionShownCount(int count) {
  pref_service_->SetInteger(prefs::kProximityAuthPromotionShownCount, count);
}

int ProximityAuthProfilePrefManager::GetPromotionShownCount() const {
  return pref_service_->GetInteger(prefs::kProximityAuthPromotionShownCount);
}

}  // namespace proximity_auth
