// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/proximity_auth_local_state_pref_manager.h"

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/values.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_pref_names.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace proximity_auth {

ProximityAuthLocalStatePrefManager::ProximityAuthLocalStatePrefManager(
    PrefService* local_state)
    : local_state_(local_state) {}

ProximityAuthLocalStatePrefManager::~ProximityAuthLocalStatePrefManager() {}

// static.
void ProximityAuthLocalStatePrefManager::RegisterPrefs(
    PrefRegistrySimple* registry) {
  // Prefs for all users are stored in a dictionary under this pref name.
  registry->RegisterDictionaryPref(prefs::kEasyUnlockLocalStateUserPrefs);

  // Most Smart Lock prefs are stored in regular user prefs, and then copied out
  // to local state for reference. This particular pref, in contrast, needs its
  // source of truth to be in the local state, because it needs to be written
  // to from the login screen.
  registry->RegisterDictionaryPref(
      prefs::kProximityAuthHasShownLoginDisabledMessage);
}

void ProximityAuthLocalStatePrefManager::SetIsEasyUnlockEnabled(
    bool is_easy_unlock_enabled) const {
  NOTREACHED();
}

void ProximityAuthLocalStatePrefManager::SetEasyUnlockEnabledStateSet() const {
  NOTREACHED();
}

void ProximityAuthLocalStatePrefManager::SetActiveUser(
    const AccountId& active_user) {
  active_user_ = active_user;
}

void ProximityAuthLocalStatePrefManager::SetLastPromotionCheckTimestampMs(
    int64_t timestamp_ms) {
  NOTREACHED();
}

int64_t ProximityAuthLocalStatePrefManager::GetLastPromotionCheckTimestampMs()
    const {
  NOTREACHED();
  return 0;
}

void ProximityAuthLocalStatePrefManager::SetPromotionShownCount(int count) {
  NOTREACHED();
}

int ProximityAuthLocalStatePrefManager::GetPromotionShownCount() const {
  NOTREACHED();
  return 0;
}

bool ProximityAuthLocalStatePrefManager::IsEasyUnlockAllowed() const {
  const base::Value::Dict* user_prefs = GetActiveUserPrefsDictionary();
  if (user_prefs) {
    absl::optional<bool> pref_value =
        user_prefs->FindBool(ash::multidevice_setup::kSmartLockAllowedPrefName);
    if (pref_value.has_value()) {
      return pref_value.value();
    }
  }
  PA_LOG(ERROR) << "Failed to get easyunlock_allowed.";
  return true;
}

bool ProximityAuthLocalStatePrefManager::IsEasyUnlockEnabled() const {
  const base::Value::Dict* user_prefs = GetActiveUserPrefsDictionary();
  if (user_prefs) {
    absl::optional<bool> pref_value =
        user_prefs->FindBool(ash::multidevice_setup::kSmartLockEnabledPrefName);
    if (pref_value.has_value()) {
      return pref_value.value();
    }
  }
  PA_LOG(ERROR) << "Failed to get easyunlock_enabled.";
  return false;
}

bool ProximityAuthLocalStatePrefManager::IsEasyUnlockEnabledStateSet() const {
  NOTREACHED();
  return false;
}

bool ProximityAuthLocalStatePrefManager::IsChromeOSLoginAllowed() const {
  const base::Value::Dict* user_prefs = GetActiveUserPrefsDictionary();
  if (user_prefs) {
    absl::optional<bool> pref_value = user_prefs->FindBool(
        ash::multidevice_setup::kSmartLockSigninAllowedPrefName);
    if (pref_value.has_value()) {
      return pref_value.value();
    }
  }
  PA_LOG(VERBOSE) << "Failed to get is_chrome_login_allowed, not disallowing";
  return true;
}

void ProximityAuthLocalStatePrefManager::SetIsChromeOSLoginEnabled(
    bool is_enabled) {
  NOTREACHED();
}

bool ProximityAuthLocalStatePrefManager::IsChromeOSLoginEnabled() const {
  const base::Value::Dict* user_prefs = GetActiveUserPrefsDictionary();
  if (user_prefs) {
    absl::optional<bool> pref_value =
        user_prefs->FindBool(prefs::kProximityAuthIsChromeOSLoginEnabled);
    if (pref_value.has_value()) {
      return pref_value.value();
    }
  }
  PA_LOG(ERROR) << "Failed to get is_chrome_login_enabled.";
  return false;
}

void ProximityAuthLocalStatePrefManager::SetHasShownLoginDisabledMessage(
    bool has_shown) {
  ScopedDictPrefUpdate update(local_state_,
                              prefs::kEasyUnlockLocalStateUserPrefs);

  // Get or create a dictionary to persist `has_shown` for `active_user_`.
  base::Value::Dict* current_user_prefs =
      update->EnsureDict(active_user_.GetUserEmail());
  current_user_prefs->Set(prefs::kProximityAuthHasShownLoginDisabledMessage,
                          has_shown);
}

bool ProximityAuthLocalStatePrefManager::HasShownLoginDisabledMessage() const {
  const base::Value::Dict* user_prefs = GetActiveUserPrefsDictionary();
  if (!user_prefs)
    return false;

  return user_prefs->FindBool(prefs::kProximityAuthHasShownLoginDisabledMessage)
      .value_or(false);
}

const base::Value::Dict*
ProximityAuthLocalStatePrefManager::GetActiveUserPrefsDictionary() const {
  if (!active_user_.is_valid()) {
    PA_LOG(ERROR) << "No active account.";
    return nullptr;
  }

  const base::Value::Dict& all_user_prefs_dict =
      local_state_->GetDict(prefs::kEasyUnlockLocalStateUserPrefs);

  const base::Value::Dict* current_user_prefs =
      all_user_prefs_dict.FindDict(active_user_.GetUserEmail());
  if (!current_user_prefs) {
    PA_LOG(ERROR) << "Failed to find prefs for current user.";
    return nullptr;
  }
  return current_user_prefs;
}

}  // namespace proximity_auth
