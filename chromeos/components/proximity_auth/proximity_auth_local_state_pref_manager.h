// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_LOCAL_STATE_PREF_MANAGER_H_
#define CHROMEOS_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_LOCAL_STATE_PREF_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chromeos/components/proximity_auth/proximity_auth_pref_manager.h"
#include "components/account_id/account_id.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class DictionaryValue;
}  // namespace base

namespace proximity_auth {

// Implementation of ProximityAuthPrefManager based on the device's local state,
// used before the user logs in. After login, ProximityAuthProfilePrefManager is
// used to manage prefs inside the user session.
// Note: All prefs managed by this class are read-only. These prefs are synced
// from each of the user's profile prefs. For privacy reasons, only a subset of
// all prefs are accessible from the local state.
class ProximityAuthLocalStatePrefManager : public ProximityAuthPrefManager {
 public:
  explicit ProximityAuthLocalStatePrefManager(PrefService* local_state);
  ~ProximityAuthLocalStatePrefManager() override;

  // Registers the prefs used by this class to the given |pref_service|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Changes the current user for whom to fetch prefs, i.e. when the focused
  // user pod changes.
  void SetActiveUser(const AccountId& active_user);
  AccountId active_user() { return active_user_; }

  // ProximityAuthPrefManager:
  bool IsEasyUnlockAllowed() const override;
  bool IsEasyUnlockEnabled() const override;
  bool IsEasyUnlockEnabledStateSet() const override;
  bool IsChromeOSLoginAllowed() const override;
  bool IsChromeOSLoginEnabled() const override;

 private:
  // ProximityAuthPrefManager:
  void SetIsEasyUnlockEnabled(bool is_easy_unlock_enabled) const override;
  void SetEasyUnlockEnabledStateSet() const override;
  void SetLastPromotionCheckTimestampMs(int64_t timestamp_ms) override;
  int64_t GetLastPromotionCheckTimestampMs() const override;
  void SetPromotionShownCount(int count) override;
  int GetPromotionShownCount() const override;
  void SetIsChromeOSLoginEnabled(bool is_enabled) override;
  void SetHasShownLoginDisabledMessage(bool has_shown) override;
  bool HasShownLoginDisabledMessage() const override;

  const base::DictionaryValue* GetActiveUserPrefsDictionary() const;

  // Contains local state preferences that outlive the lifetime of this object
  // and across process restarts. Not owned and must outlive this instance.
  PrefService* local_state_;

  // The account id of the active user for which to fetch the prefs.
  AccountId active_user_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthLocalStatePrefManager);
};

}  // namespace proximity_auth

#endif  // CHROMEOS_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_LOCAL_STATE_PREF_MANAGER_H_
