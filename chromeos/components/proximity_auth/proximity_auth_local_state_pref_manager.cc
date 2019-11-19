// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/proximity_auth_local_state_pref_manager.h"

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/proximity_auth/proximity_auth_pref_names.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
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
  bool pref_value;
  const base::DictionaryValue* user_prefs = GetActiveUserPrefsDictionary();
  if (!user_prefs || !user_prefs->GetBooleanWithoutPathExpansion(
                         chromeos::multidevice_setup::kSmartLockAllowedPrefName,
                         &pref_value)) {
    PA_LOG(ERROR) << "Failed to get easyunlock_allowed.";
    return true;
  }
  return pref_value;
}

bool ProximityAuthLocalStatePrefManager::IsEasyUnlockEnabled() const {
  bool pref_value;
  const base::DictionaryValue* user_prefs = GetActiveUserPrefsDictionary();
  if (!user_prefs || !user_prefs->GetBooleanWithoutPathExpansion(
                         chromeos::multidevice_setup::kSmartLockEnabledPrefName,
                         &pref_value)) {
    PA_LOG(ERROR) << "Failed to get easyunlock_enabled.";
    return false;
  }
  return pref_value;
}

bool ProximityAuthLocalStatePrefManager::IsEasyUnlockEnabledStateSet() const {
  NOTREACHED();
  return false;
}

bool ProximityAuthLocalStatePrefManager::IsChromeOSLoginAllowed() const {
  bool pref_value;
  const base::DictionaryValue* user_prefs = GetActiveUserPrefsDictionary();
  if (!user_prefs ||
      !user_prefs->GetBooleanWithoutPathExpansion(
          chromeos::multidevice_setup::kSmartLockSigninAllowedPrefName,
          &pref_value)) {
    PA_LOG(VERBOSE) << "Failed to get is_chrome_login_allowed, not disallowing";
    return true;
  }
  return pref_value;
}

void ProximityAuthLocalStatePrefManager::SetIsChromeOSLoginEnabled(
    bool is_enabled) {
  NOTREACHED();
}

bool ProximityAuthLocalStatePrefManager::IsChromeOSLoginEnabled() const {
  bool pref_value;
  const base::DictionaryValue* user_prefs = GetActiveUserPrefsDictionary();
  if (!user_prefs ||
      !user_prefs->GetBooleanWithoutPathExpansion(
          prefs::kProximityAuthIsChromeOSLoginEnabled, &pref_value)) {
    PA_LOG(ERROR) << "Failed to get is_chrome_login_enabled.";
    return false;
  }
  return pref_value;
}

void ProximityAuthLocalStatePrefManager::SetHasShownLoginDisabledMessage(
    bool has_shown) {
  DictionaryPrefUpdate update(local_state_,
                              prefs::kEasyUnlockLocalStateUserPrefs);

  base::DictionaryValue* current_user_prefs;
  update.Get()->GetDictionaryWithoutPathExpansion(active_user_.GetUserEmail(),
                                                  &current_user_prefs);
  if (current_user_prefs) {
    current_user_prefs->SetKey(
        prefs::kProximityAuthHasShownLoginDisabledMessage,
        base::Value(has_shown));
    return;
  }

  // Create an otherwise empty dictionary in order to ensure |has_shown| is
  // persisted for |active_user_|.
  std::unique_ptr<base::DictionaryValue> new_current_user_prefs =
      std::make_unique<base::DictionaryValue>();
  new_current_user_prefs->SetKey(
      prefs::kProximityAuthHasShownLoginDisabledMessage,
      base::Value(has_shown));
  update->SetWithoutPathExpansion(active_user_.GetUserEmail(),
                                  std::move(new_current_user_prefs));
}

bool ProximityAuthLocalStatePrefManager::HasShownLoginDisabledMessage() const {
  const base::DictionaryValue* user_prefs = GetActiveUserPrefsDictionary();
  if (!user_prefs)
    return false;

  bool pref_value = false;
  user_prefs->GetBooleanWithoutPathExpansion(
      prefs::kProximityAuthHasShownLoginDisabledMessage, &pref_value);
  return pref_value;
}

const base::DictionaryValue*
ProximityAuthLocalStatePrefManager::GetActiveUserPrefsDictionary() const {
  if (!active_user_.is_valid()) {
    PA_LOG(ERROR) << "No active account.";
    return nullptr;
  }

  const base::DictionaryValue* all_user_prefs_dict =
      local_state_->GetDictionary(prefs::kEasyUnlockLocalStateUserPrefs);
  DCHECK(all_user_prefs_dict);

  const base::DictionaryValue* current_user_prefs;
  if (!all_user_prefs_dict->GetDictionaryWithoutPathExpansion(
          active_user_.GetUserEmail(), &current_user_prefs)) {
    PA_LOG(ERROR) << "Failed to find prefs for current user.";
    return nullptr;
  }
  return current_user_prefs;
}

}  // namespace proximity_auth
