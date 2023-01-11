// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PROFILE_PREF_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PROFILE_PREF_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_pref_manager.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace proximity_auth {

// Implementation of ProximityAuthPrefManager for a logged in session with a
// user profile.
class ProximityAuthProfilePrefManager
    : public ProximityAuthPrefManager,
      public ash::multidevice_setup::MultiDeviceSetupClient::Observer {
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

  ~ProximityAuthProfilePrefManager() override;

  // Initializes the manager to listen to pref changes and sync prefs to the
  // user's local state.
  void StartSyncingToLocalState(PrefService* local_state,
                                const AccountId& account_id);

  // Registers the prefs used by this class to the given |pref_service|.
  static void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry);

  // ProximityAuthPrefManager:
  bool IsEasyUnlockAllowed() const override;
  void SetIsEasyUnlockEnabled(bool is_easy_unlock_enabled) const override;
  bool IsEasyUnlockEnabled() const override;
  void SetEasyUnlockEnabledStateSet() const override;
  bool IsEasyUnlockEnabledStateSet() const override;
  void SetLastPromotionCheckTimestampMs(int64_t timestamp_ms) override;
  int64_t GetLastPromotionCheckTimestampMs() const override;
  void SetPromotionShownCount(int count) override;
  int GetPromotionShownCount() const override;
  bool IsChromeOSLoginAllowed() const override;
  void SetIsChromeOSLoginEnabled(bool is_enabled) override;
  bool IsChromeOSLoginEnabled() const override;
  void SetHasShownLoginDisabledMessage(bool has_shown) override;
  bool HasShownLoginDisabledMessage() const override;

  // ash::multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnFeatureStatesChanged(
      const ash::multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

 private:
  void SyncPrefsToLocalState();

  // Contains perferences that outlive the lifetime of this object and across
  // process restarts. Not owned and must outlive this instance.
  PrefService* pref_service_ = nullptr;

  // Listens to pref changes so they can be synced to the local state.
  PrefChangeRegistrar registrar_;

  // The local state to which to sync the profile prefs.
  PrefService* local_state_ = nullptr;

  // The account id of the current profile.
  AccountId account_id_;

  // Used to determine the FeatureState of Smart Lock.
  ash::multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_ =
      nullptr;

  base::WeakPtrFactory<ProximityAuthProfilePrefManager> weak_ptr_factory_{this};
};

}  // namespace proximity_auth

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PROFILE_PREF_MANAGER_H_
