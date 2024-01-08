// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PROFILE_PREF_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PROFILE_PREF_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/account_id/account_id.h"

class PrefService;

namespace ash::multidevice_setup {
class MultiDeviceSetupClient;
}  // namespace ash::multidevice_setup

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace proximity_auth {

// Interface for setting and getting persistent user preferences for a logged in
// session with a user profile.
class ProximityAuthProfilePrefManager {
 public:
  // Creates a pref manager backed by preferences registered in
  // |pref_service| (persistent across browser restarts). |pref_service| should
  // have been registered using RegisterPrefs(). Not owned, must out live this
  // instance.
  ProximityAuthProfilePrefManager(
      PrefService* pref_service,
      ash::multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);

  ProximityAuthProfilePrefManager(const ProximityAuthProfilePrefManager&) =
      delete;
  ProximityAuthProfilePrefManager& operator=(
      const ProximityAuthProfilePrefManager&) = delete;

  ~ProximityAuthProfilePrefManager();

  // Registers the prefs used by this class to the given |pref_service|.
  static void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns true if SmartLock/EasyUnlock is allowed. Note: there is no
  // corresponding setter because this pref is pushed through an enterprise
  // policy. Note that this pref completely disables EasyUnlock, hiding even the
  // UI. See IsEasyUnlockEnabled() for comparison.
  bool IsEasyUnlockAllowed() const;

  // Returns true if SmartLock/EasyUnlock is enabled, i.e. the user has gone
  // through the setup flow and has at least one phone as an unlock key. Compare
  // to IsEasyUnlockAllowed(), which completely removes the feature from
  // existence.
  void SetIsEasyUnlockEnabled(bool is_easy_unlock_enabled) const;
  bool IsEasyUnlockEnabled() const;

  // Returns true if SmartLock/EasyUnlock has ever been enabled, regardless of
  // whether the feature is currently enabled or disabled. Compare to
  // IsEasyUnlockEnabled(), which flags the latter case.
  void SetEasyUnlockEnabledStateSet() const;
  bool IsEasyUnlockEnabledStateSet() const;

  // Setter and getter for the timestamp of the last time the promotion was
  // shown to the user.
  void SetLastPromotionCheckTimestampMs(int64_t timestamp_ms);
  int64_t GetLastPromotionCheckTimestampMs() const;

  // Setter and getter for the number of times the promotion was shown to the
  // user.
  void SetPromotionShownCount(int count);
  int GetPromotionShownCount() const;

 private:
  // Contains perferences that outlive the lifetime of this object and across
  // process restarts. Not owned and must outlive this instance.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // The account id of the current profile.
  AccountId account_id_;

  // Used to determine the FeatureState of Smart Lock.
  raw_ptr<ash::multidevice_setup::MultiDeviceSetupClient>
      multidevice_setup_client_ = nullptr;

  base::WeakPtrFactory<ProximityAuthProfilePrefManager> weak_ptr_factory_{this};
};

}  // namespace proximity_auth

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PROFILE_PREF_MANAGER_H_
