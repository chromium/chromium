// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_DATA_CLEANER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_DATA_CLEANER_H_

#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"
#include "components/sync/base/model_type.h"

class PrefService;

namespace autofill {

class PersonalDataManager;

// AddressDataCleaner is responsible for applying address cleanups once
// on browser startup provided that the sync is enabled or when the sync starts.
class AddressDataCleaner {
 public:
  AddressDataCleaner(
      PersonalDataManager* personal_data_manager,
      AlternativeStateNameMapUpdater* alternative_state_name_map_updater,
      PrefService* pref_service);
  ~AddressDataCleaner();
  AddressDataCleaner(const AddressDataCleaner&) = delete;
  AddressDataCleaner& operator=(const AddressDataCleaner&) =
      delete;

  // Applies address cleanups if sync is disabled.
  void MaybeCleanupAddressData();

  // Applies address cleanups if all address Autofill `model_type`s are enabled.
  void MaybeCleanupAddressDataAfterSyncChange(syncer::ModelType model_type);

 protected:
  friend class AddressDataCleanerTest;

 private:
  // Shared implementation of `MaybeCleanupAddressData()` and
  // `MaybeCleanupAddressDataAfterSyncChange()`
  void ApplyAddressFixesAndCleanups();

  // Applies the deduping routine once per major version. Calls DedupeProfiles()
  // with the content of `PersonalDataManager::GetProfiles()` as a parameter.
  // Removes the profiles to delete from the database and updates the others.
  // Returns true if the routine was run.
  bool ApplyAddressDedupingRoutine();

  // Goes through all the `existing_profiles` and merges all similar profiles
  // together. All the profiles except the results of the merges will be
  // added to `profile_guids_to_delete`. This routine should be run once per
  // major version.
  //
  // This method should only be called by ApplyDedupingRoutine(). It is split
  // for testing purposes.
  void DedupeProfiles(
      std::vector<std::unique_ptr<AutofillProfile>>* existing_profiles,
      std::unordered_set<std::string>* profile_guids_to_delete) const;

  // Tries to delete disused addresses on startup.
  bool DeleteDisusedAddresses();

  // True if autofill profile dedupe needs to be performed.
  bool is_autofill_profile_dedupe_pending_ = true;

  // True if the profile cleanups need to be performed.
  bool is_profile_cleanup_pending_ = true;

  // The personal data manager, used to load and update the personal data
  // from/to the web database.
  const raw_ptr<PersonalDataManager> personal_data_manager_ = nullptr;

  // The PrefService used by this instance.
  const raw_ptr<PrefService> pref_service_ = nullptr;

  // The AlternativeStateNameMapUpdater, used to populate
  // AlternativeStateNameMap with the geographical state data.
  const raw_ptr<AlternativeStateNameMapUpdater>
      alternative_state_name_map_updater_ = nullptr;

  // Profiles are loaded through two sync bridges, the AUTOFILL_PROFILE and the
  // CONTACT_INFO sync bridge. Both of them are behind the same settings toggle,
  // so it is guaranteed that either both of them or none of them are activated
  // together. In order to initialize the alternative state map only after
  // profiles from both sources were loaded, we track whether each model type's
  // `SyncStarted()` event has triggered yet.
  // TODO(crbug.com/1348294): Remove once the AUTOFILL_PROFILE sync bridge is
  // deprecated.
  bool autofill_profile_sync_started_ = false;
  bool contact_info_sync_started_ = false;

  // base::WeakPtr ensures that the callback bound to the object is canceled
  // when that object is destroyed.
  base::WeakPtrFactory<AddressDataCleaner> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_DATA_CLEANER_H_
