// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ADDRESS_DATA_CLEANER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ADDRESS_DATA_CLEANER_H_

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager_observer.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service_observer.h"

class PrefService;
namespace syncer {
class SyncService;
}

namespace autofill {

// AddressDataCleaner is responsible for applying address cleanups on browser
// startup, after sync is ready (if applicable).
class AddressDataCleaner : public AddressDataManager::Observer,
                           public syncer::SyncServiceObserver {
 public:
  AddressDataCleaner(
      AddressDataManager& address_data_manager,
      syncer::SyncService* sync_service,
      PrefService& pref_service,
      AlternativeStateNameMapUpdater* alternative_state_name_map_updater);
  ~AddressDataCleaner() override;
  AddressDataCleaner(const AddressDataCleaner&) = delete;
  AddressDataCleaner& operator=(const AddressDataCleaner&) = delete;

  // Determines whether the cleanups should run depending on the sync state and
  // runs them if applicable. Ensures that the cleanups are run at most once
  // over multiple invocations of the functions.
  // Deduplication is particularly expensive, since it runs in O(#profiles^2).
  // For this reason, it is only run once per milestone.
  void MaybeCleanupAddressData();

  // Computes the `comparator.NonMergeableSettingVisibleTypes()` between
  // `profile` and every element of `other_profiles`. Returns the subset of them
  // that have minimum size combined with a profile that was used to obtain
  // them. Thus, all elements of the returned vector have the same size. In
  // profile deduplication context, this indicates what is the minimum sets of
  // types are whose removal makes `profile` a duplicate with specific other
  // profile in `other_profiles`. The returned type
  // sets in the returned vector are not unique and may contain duplicates. This
  // is intentional, as this means the same set of types prevented deduplication
  // in multiple cases, which is used to weight metrics. Profile pairs of
  // different countries are ignored. See `NonMergeableSettingVisibleTypes()`.
  // As such, the returned vector is empty if no profile in `other_profiles` has
  // the same country as `profile`.
  static std::vector<autofill_metrics::DifferingProfileWithTypeSet>
  CalculateMinimalIncompatibleProfileWithTypeSets(
      const AutofillProfile& profile,
      base::span<const AutofillProfile* const> existing_profiles,
      const AutofillProfileComparator& comparator);

 protected:
  // Specifies the deferred database operation to execute for a given profile
  // once all cleanup phases have finished.
  enum class ProfileAction { kNone = 0, kUpdate = 1, kRemove = 2 };

  // Represents an AutofillProfile paired with a deferred database operation.
  // Used to accumulate local updates and removals during cleanup routines (e.g.
  // disused profiles and deduplication).
  struct ProfileWithAction {
    AutofillProfile profile;
    ProfileAction action = ProfileAction::kNone;
  };

 private:
  friend class AddressDataCleanerTestApi;

  // Deduplicates the PDMs profiles, by merging profile pairs where one is a
  // subset of the other. Account profiles are never deduplicated.
  // Modifies `profiles` in place to reflect the merged data. Profiles to be
  // removed are not deleted from the vector but are marked in
  // `profiles_action`. Virtual for testing.
  virtual void ApplyDeduplicationRoutine(
      std::vector<ProfileWithAction>& profiles);

  // Migrates the phonetic names that were stored in the regular name fields to
  // alternative name fields. Modifies `profiles` in place to reflect the
  // migrated phonetic names.
  // TODO(crbug.com/359768803): Remove this method once the migration is done.
  virtual void MarkProfilesForPhoneticNameMigration(
      std::vector<ProfileWithAction>& profiles);

  // Mark profiles from `profiles` that were unused for at least
  // `kDisusedDataModelDeletionTimeDelta` for deletion.
  void MarkDisusedProfilesForDeletion(std::vector<ProfileWithAction>& profiles);

  // AddressDataManager::Observer
  void OnAddressDataChanged() override;

  // syncer::SyncServiceObserver
  void OnStateChanged(syncer::SyncService* sync_service) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // Iterates over `profiles` and executes the pending database
  // operations (Update/Remove) through the AddressDataManager.
  void ApplyProfileActions(const std::vector<ProfileWithAction>& profiles);

  // Merges mergeable profiles in `profiles_with_action`. This supports both
  // local and account profiles and preserves the `initial_creator_id`. The
  // algorithm proceeds in two steps, such that the amount of retained
  // information is maximized without sending the data to the account if it was
  // stored locally. Note that due to normalisation, etc, even if `IsSubsetOf()`
  // is true, the information present in the subset can still look slightly
  // different from the superset and is therefore not silently merged.
  // 1) Marks all profiles that are subsets of another profile for removal.
  //    If a profile is a subset of multiple other profiles, its usage history
  //    is merged with all of them and they are marked for update. The silent
  //    updates that the subset may have contained are intentionally dropped,
  //    such that this information is not uploaded to the account without
  //    consent. For exact duplicates, keeping the account profile is preferred.
  // 2) Merges pairs of mergeable profiles into each other.
  //    To prevent silently introducing new information into the account,
  //    local profiles are never merged into account profiles. The subsumed
  //    profile is marked for removal and the merged profile is marked for
  //    update.
  void DeduplicateProfiles(
      const AutofillProfileComparator& comparator,
      std::vector<ProfileWithAction>& profiles_with_action);

  // Used to ensure that cleanups are only performed once per profile startup.
  bool are_cleanups_pending_ = true;

  const raw_ref<AddressDataManager> address_data_manager_;
  const raw_ptr<syncer::SyncService> sync_service_;
  // Used to check whether deduplication was already run this milestone.
  const raw_ref<PrefService> pref_service_;

  // Used to ensure that the alternative state name map gets populated before
  // performing deduplication.
  const raw_ptr<AlternativeStateNameMapUpdater>
      alternative_state_name_map_updater_;

  // Observe the ADM, so cleanups can run when the data was loaded from the DB.
  base::ScopedObservation<AddressDataManager, AddressDataManager::Observer>
      adm_observer_{this};

  // Observer Sync, so cleanups are not run before any new data was synced down
  // on startup.
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observer_{this};

  // base::WeakPtr ensures that the callback bound to the object is canceled
  // when that object is destroyed.
  base::WeakPtrFactory<AddressDataCleaner> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ADDRESS_DATA_CLEANER_H_
