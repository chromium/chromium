// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/proximity_auth_profile_pref_manager.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/values.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/components/proximity_auth/proximity_auth_pref_names.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace proximity_auth {

ProximityAuthProfilePrefManager::ProximityAuthProfilePrefManager(
    PrefService* pref_service,
    chromeos::multidevice_setup::MultiDeviceSetupClient*
        multidevice_setup_client)
    : pref_service_(pref_service),
      multidevice_setup_client_(multidevice_setup_client),
      weak_ptr_factory_(this) {
  if (base::FeatureList::IsEnabled(
          chromeos::features::kEnableUnifiedMultiDeviceSetup)) {
    OnFeatureStatesChanged(multidevice_setup_client_->GetFeatureStates());

    multidevice_setup_client_->AddObserver(this);
  }
}

ProximityAuthProfilePrefManager::~ProximityAuthProfilePrefManager() {
  registrar_.RemoveAll();

  if (base::FeatureList::IsEnabled(
          chromeos::features::kEnableUnifiedMultiDeviceSetup)) {
    multidevice_setup_client_->RemoveObserver(this);
  }
}

// static
void ProximityAuthProfilePrefManager::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kEasyUnlockEnabledStateSet, false);
  registry->RegisterInt64Pref(
      prefs::kProximityAuthLastPromotionCheckTimestampMs, 0L);
  registry->RegisterIntegerPref(prefs::kProximityAuthPromotionShownCount, 0);
  registry->RegisterDictionaryPref(prefs::kProximityAuthRemoteBleDevices);
  registry->RegisterIntegerPref(
      prefs::kEasyUnlockProximityThreshold, 1,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kProximityAuthIsChromeOSLoginEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
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

  base::Closure on_pref_changed_callback =
      base::Bind(&ProximityAuthProfilePrefManager::SyncPrefsToLocalState,
                 weak_ptr_factory_.GetWeakPtr());

  registrar_.Init(pref_service_);
  registrar_.Add(chromeos::multidevice_setup::kSmartLockAllowedPrefName,
                 on_pref_changed_callback);
  registrar_.Add(
      chromeos::multidevice_setup::kSmartLockEnabledDeprecatedPrefName,
      on_pref_changed_callback);
  registrar_.Add(proximity_auth::prefs::kEasyUnlockProximityThreshold,
                 on_pref_changed_callback);
  registrar_.Add(proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled,
                 on_pref_changed_callback);
  registrar_.Add(chromeos::multidevice_setup::kSmartLockSigninAllowedPrefName,
                 on_pref_changed_callback);

  SyncPrefsToLocalState();
}

void ProximityAuthProfilePrefManager::SyncPrefsToLocalState() {
  std::unique_ptr<base::DictionaryValue> user_prefs_dict(
      new base::DictionaryValue());

  user_prefs_dict->SetKey(
      chromeos::multidevice_setup::kSmartLockAllowedPrefName,
      base::Value(IsEasyUnlockAllowed()));
  user_prefs_dict->SetKey(
      chromeos::multidevice_setup::kSmartLockEnabledPrefName,
      base::Value(IsEasyUnlockEnabled()));
  user_prefs_dict->SetKey(prefs::kEasyUnlockProximityThreshold,
                          base::Value(GetProximityThreshold()));
  user_prefs_dict->SetKey(prefs::kProximityAuthIsChromeOSLoginEnabled,
                          base::Value(IsChromeOSLoginEnabled()));
  user_prefs_dict->SetKey(
      chromeos::multidevice_setup::kSmartLockSigninAllowedPrefName,
      base::Value(IsChromeOSLoginAllowed()));

  DictionaryPrefUpdate update(local_state_,
                              prefs::kEasyUnlockLocalStateUserPrefs);
  update->SetWithoutPathExpansion(account_id_.GetUserEmail(),
                                  std::move(user_prefs_dict));
}

bool ProximityAuthProfilePrefManager::IsEasyUnlockAllowed() const {
  return pref_service_->GetBoolean(
      chromeos::multidevice_setup::kSmartLockAllowedPrefName);
}

void ProximityAuthProfilePrefManager::SetIsEasyUnlockEnabled(
    bool is_easy_unlock_enabled) const {
  pref_service_->SetBoolean(
      chromeos::multidevice_setup::kSmartLockEnabledDeprecatedPrefName,
      is_easy_unlock_enabled);
}

bool ProximityAuthProfilePrefManager::IsEasyUnlockEnabled() const {
  if (base::FeatureList::IsEnabled(
          chromeos::features::kEnableUnifiedMultiDeviceSetup) &&
      !is_in_legacy_host_mode_) {
    return feature_state_ ==
           chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser;
  }

  return pref_service_->GetBoolean(
      chromeos::multidevice_setup::kSmartLockEnabledDeprecatedPrefName);
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

void ProximityAuthProfilePrefManager::SetProximityThreshold(
    ProximityThreshold value) {
  pref_service_->SetInteger(prefs::kEasyUnlockProximityThreshold, value);
}

ProximityAuthProfilePrefManager::ProximityThreshold
ProximityAuthProfilePrefManager::GetProximityThreshold() const {
  int pref_value =
      pref_service_->GetInteger(prefs::kEasyUnlockProximityThreshold);
  return static_cast<ProximityThreshold>(pref_value);
}

bool ProximityAuthProfilePrefManager::IsChromeOSLoginAllowed() const {
  return pref_service_->GetBoolean(
      chromeos::multidevice_setup::kSmartLockSigninAllowedPrefName);
}

void ProximityAuthProfilePrefManager::SetIsChromeOSLoginEnabled(
    bool is_enabled) {
  return pref_service_->SetBoolean(prefs::kProximityAuthIsChromeOSLoginEnabled,
                                   is_enabled);
}

bool ProximityAuthProfilePrefManager::IsChromeOSLoginEnabled() const {
  return pref_service_->GetBoolean(prefs::kProximityAuthIsChromeOSLoginEnabled);
}

void ProximityAuthProfilePrefManager::OnFeatureStatesChanged(
    const chromeos::multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  const auto it = feature_states_map.find(
      chromeos::multidevice_setup::mojom::Feature::kSmartLock);
  if (it == feature_states_map.end()) {
    feature_state_ = chromeos::multidevice_setup::mojom::FeatureState::
        kUnavailableNoVerifiedHost;
    return;
  }
  feature_state_ = it->second;

  if (local_state_ && account_id_.is_valid())
    SyncPrefsToLocalState();
}

void ProximityAuthProfilePrefManager::SetIsInLegacyHostMode(
    bool is_in_legacy_host_mode) {
  is_in_legacy_host_mode_ = is_in_legacy_host_mode;

  if (local_state_ && account_id_.is_valid())
    SyncPrefsToLocalState();
}

}  // namespace proximity_auth
