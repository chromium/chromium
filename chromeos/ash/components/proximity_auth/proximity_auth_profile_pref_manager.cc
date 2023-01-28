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
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace proximity_auth {

ProximityAuthProfilePrefManager::ProximityAuthProfilePrefManager(
    PrefService* pref_service,
    ash::multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : pref_service_(pref_service),
      multidevice_setup_client_(multidevice_setup_client) {
  OnFeatureStatesChanged(multidevice_setup_client_->GetFeatureStates());

  multidevice_setup_client_->AddObserver(this);
}

ProximityAuthProfilePrefManager::~ProximityAuthProfilePrefManager() {
  registrar_.RemoveAll();

  multidevice_setup_client_->RemoveObserver(this);
}

// static
void ProximityAuthProfilePrefManager::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kEasyUnlockEnabledStateSet, false);
  registry->RegisterInt64Pref(
      prefs::kProximityAuthLastPromotionCheckTimestampMs, 0L);
  registry->RegisterIntegerPref(prefs::kProximityAuthPromotionShownCount, 0);
  registry->RegisterDictionaryPref(prefs::kProximityAuthRemoteBleDevices);
  registry->RegisterBooleanPref(
      prefs::kProximityAuthIsChromeOSLoginEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

void ProximityAuthProfilePrefManager::StartSyncingToLocalState(
    PrefService* local_state,
    const AccountId& account_id) {
  local_state_ = local_state;
  account_id_ = account_id;

  if (!account_id_.is_valid()) {
    PA_LOG(ERROR) << "Invalid account_id.";
    return;
  }

  auto on_pref_changed_callback = base::BindRepeating(
      &ProximityAuthProfilePrefManager::SyncPrefsToLocalState,
      weak_ptr_factory_.GetWeakPtr());

  registrar_.Init(pref_service_);
  registrar_.Add(ash::multidevice_setup::kSmartLockAllowedPrefName,
                 on_pref_changed_callback);
  registrar_.Add(ash::multidevice_setup::kSmartLockEnabledDeprecatedPrefName,
                 on_pref_changed_callback);
  registrar_.Add(proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled,
                 on_pref_changed_callback);
  registrar_.Add(ash::multidevice_setup::kSmartLockSigninAllowedPrefName,
                 on_pref_changed_callback);

  SyncPrefsToLocalState();
}

void ProximityAuthProfilePrefManager::SyncPrefsToLocalState() {
  base::Value user_prefs_dict(base::Value::Type::DICT);

  user_prefs_dict.SetBoolKey(ash::multidevice_setup::kSmartLockAllowedPrefName,
                             IsEasyUnlockAllowed());
  user_prefs_dict.SetBoolKey(ash::multidevice_setup::kSmartLockEnabledPrefName,
                             IsEasyUnlockEnabled());
  user_prefs_dict.SetBoolKey(
      ash::multidevice_setup::kSmartLockSigninAllowedPrefName,
      IsChromeOSLoginAllowed());
  user_prefs_dict.SetBoolKey(prefs::kProximityAuthIsChromeOSLoginEnabled,
                             IsChromeOSLoginEnabled());

  // If Signin with Smart Lock is enabled, then the "has shown Signin with
  // Smart Lock is disabled message" flag should be false, to ensure the message
  // is displayed if Signin with Smart Lock is disabled. Otherwise, copy the
  // old value.
  bool has_shown_login_disabled_message =
      IsChromeOSLoginEnabled() ? false : HasShownLoginDisabledMessage();
  user_prefs_dict.SetBoolKey(prefs::kProximityAuthHasShownLoginDisabledMessage,
                             has_shown_login_disabled_message);

  ScopedDictPrefUpdate update(local_state_,
                              prefs::kEasyUnlockLocalStateUserPrefs);
  update->Set(account_id_.GetUserEmail(), std::move(user_prefs_dict));
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

bool ProximityAuthProfilePrefManager::IsChromeOSLoginAllowed() const {
  return pref_service_->GetBoolean(
      ash::multidevice_setup::kSmartLockSigninAllowedPrefName);
}

void ProximityAuthProfilePrefManager::SetIsChromeOSLoginEnabled(
    bool is_enabled) {
  return pref_service_->SetBoolean(prefs::kProximityAuthIsChromeOSLoginEnabled,
                                   is_enabled);
}

bool ProximityAuthProfilePrefManager::IsChromeOSLoginEnabled() const {
  return pref_service_->GetBoolean(prefs::kProximityAuthIsChromeOSLoginEnabled);
}

void ProximityAuthProfilePrefManager::SetHasShownLoginDisabledMessage(
    bool has_shown) {
  // This is persisted within SyncPrefsToLocalState() instead, since the local
  // state must act as the source of truth for this pref.

  // TODO(crbug.com/1152491): Add a NOTREACHED() to ensure this method is not
  // called. It is currently incorrectly, though harmlessly, called by virtual
  // Chrome OS on Linux.
}

bool ProximityAuthProfilePrefManager::HasShownLoginDisabledMessage() const {
  const base::Value::Dict& all_user_prefs_dict =
      local_state_->GetDict(prefs::kEasyUnlockLocalStateUserPrefs);
  const base::Value::Dict* current_user_prefs =
      all_user_prefs_dict.FindDict(account_id_.GetUserEmail());
  if (!current_user_prefs) {
    PA_LOG(ERROR) << "Failed to find local state prefs for current user.";
    return false;
  }

  return current_user_prefs
      ->FindBool(prefs::kProximityAuthHasShownLoginDisabledMessage)
      .value_or(false);
}

void ProximityAuthProfilePrefManager::OnFeatureStatesChanged(
    const ash::multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  if (local_state_ && account_id_.is_valid())
    SyncPrefsToLocalState();
}

}  // namespace proximity_auth
