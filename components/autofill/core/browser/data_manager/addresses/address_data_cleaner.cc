// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/addresses/address_data_cleaner.h"

#include <algorithm>

#include "base/containers/to_vector.h"
#include "base/notreached.h"
#include "base/version_info/version_info.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_token_quality.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/address_data_cleaner_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace autofill {

namespace {

using DifferingProfileWithTypeSet =
    autofill_metrics::DifferingProfileWithTypeSet;

// Determines whether cleanups should be deferred because the latest data wasn't
// synced down yet.
bool ShouldWaitForSync(syncer::SyncService* sync_service) {
  // No need to wait if the user is not syncing addresses.
  if (!sync_service || !sync_service->GetUserSettings()->GetSelectedTypes().Has(
                           syncer::UserSelectableType::kAutofill)) {
    return false;
  }

  auto should_wait = [&sync_service](syncer::DataType data_type) {
    switch (sync_service->GetDownloadStatusFor(data_type)) {
      case syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates:
        return true;
      case syncer::SyncService::DataTypeDownloadStatus::kUpToDate:
      // If the download status is kError, it will likely not become available
      // anytime soon. In this case, don't defer the cleanups.
      case syncer::SyncService::DataTypeDownloadStatus::kError:
        return false;
    }
    NOTREACHED();
  };
  return should_wait(syncer::DataType::AUTOFILL_PROFILE) ||
         should_wait(syncer::DataType::CONTACT_INFO);
}

// - Merges local profiles occurring earlier in `profiles` with mergeable other
//   local profiles later in `profiles`, deleting the earlier one.
// - Deletes local profiles that are subsets of account profiles.
// Mergability is determined using `comparator`.
// TODO(crbug.com/357074792): Remove once support for deduplicating account
// profiles is launched. While both exist please make sure to keep them in sync.
void DeduplicateProfiles(const AutofillProfileComparator& comparator,
                         std::vector<AutofillProfile> profiles,
                         AddressDataManager& adm) {
  // Partition the profiles into local and account profiles:
  // - Local: [profiles.begin(), account_profiles.begin()[
  // - Account: account_profiles
  auto account_profiles = std::ranges::stable_partition(
      profiles, std::not_fn(&AutofillProfile::IsAccountProfile));

  size_t num_profiles_deleted = 0;
  for (auto local_profile_it = profiles.begin();
       local_profile_it != account_profiles.begin(); ++local_profile_it) {
    // If possible, merge `*local_profile_it` with another local profile and
    // remove it.
    if (auto merge_candidate = std::ranges::find_if(
            local_profile_it + 1, account_profiles.begin(),
            [&](const AutofillProfile& local_profile2) {
              return comparator.AreMergeable(*local_profile_it, local_profile2);
            });
        merge_candidate != account_profiles.begin()) {
      merge_candidate->MergeDataFrom(*local_profile_it,
                                     comparator.app_locale());
      adm.UpdateProfile(*merge_candidate);
      adm.RemoveProfile(local_profile_it->guid(),
                        /*non_permanent_account_profile_removal=*/true);
      num_profiles_deleted++;
      continue;
    }
    // `*local_profile_it` is not mergeable with another local profile. But it
    // might be a subset of an account profile and can thus be removed.
    if (auto superset_account_profile = std::ranges::find_if(
            account_profiles,
            [&](const AutofillProfile& account_profile) {
              return comparator.AreMergeable(*local_profile_it,
                                             account_profile) &&
                     local_profile_it->IsSubsetOf(comparator, account_profile);
            });
        superset_account_profile != profiles.end()) {
      adm.RemoveProfile(local_profile_it->guid(),
                        /*non_permanent_account_profile_removal=*/true);
      num_profiles_deleted++;
      // Account profiles track from which service they originate. This allows
      // Autofill to distinguish between Chrome and non-Chrome account
      // profiles and measure the added utility of non-Chrome profiles. Since
      // the `superset_account_profile` matched the information that was already
      // present in Autofill (`*local_profile_it`), the account profile doesn't
      // provide any utility. To capture this in the metric, the merged
      // profile is treated as a Chrome account profile.
      superset_account_profile->set_initial_creator_id(
          AutofillProfile::kInitialCreatorOrModifierChrome);
      superset_account_profile->set_last_modifier_id(
          AutofillProfile::kInitialCreatorOrModifierChrome);
      adm.UpdateProfile(*superset_account_profile);
      continue;
    }
  }
  autofill_metrics::LogNumberOfProfilesRemovedDuringDedupe(
      num_profiles_deleted);
}

// Merges mergeable profiles in the `profiles` and deletes the subsets.
// Unlike `DeduplicateProfiles()`, this supports both local and account profiles
// and preserves the `initial_creator_id`.
// The algorithm proceeds in two steps, such that the amount of retained
// information is maximized without sending the data to the account if it was
// stored locally. Note that due to normalisation, etc, even if `IsSubsetOf()`
// is true, the information present in the subset can still look slightly
// different from the superset and is therefore not silently merged. 1) Removes
// all profiles that are subsets of another profile.
//   If a profile is a subset of multiple other profiles, its usage history is
//   merged with all of them. The silent updates that the subset may have
//   contained are intentionally dropped, such that this information is not
//   uploaded to the account without consent. For exact duplicates, keeping the
//   account profile is preferred.
// 2) Merges pairs of mergeable profiles into each other.
//   To prevent silently introducing new information into the account,
//   local profiles are never merged into account profiles.
// TODO(crbug.com/357074792): Once the feature is launched, remove the
// `DeduplicateProfiles()` function and rename this function to
// `DeduplicateProfiles()`.
void DeduplicateWithAccountProfiles(const AutofillProfileComparator& comparator,
                                    std::vector<AutofillProfile> profiles,
                                    AddressDataManager& adm) {
  std::set<std::string> guids_to_delete;

  for (const AutofillProfile& profile : profiles) {
    // Returns true if `profile` is a subset of `superset`.
    auto is_subset = [&](const AutofillProfile& superset) {
      if (!profile.IsSubsetOf(comparator, superset)) {
        return false;
      }
      if (!superset.IsSubsetOf(comparator, profile)) {
        // `profile` is a strict subset of `other_profile`.
        return true;
      }
      if (profile.record_type() != superset.record_type()) {
        return profile.record_type() ==
               AutofillProfile::RecordType::kLocalOrSyncable;
      }

      return profile.guid() < superset.guid();
    };

    for (AutofillProfile& superset : profiles) {
      if (guids_to_delete.contains(superset.guid()) || !is_subset(superset)) {
        continue;
      }
      guids_to_delete.insert(profile.guid());
      superset.usage_history().MergeUsageHistories(profile.usage_history());
      adm.UpdateProfile(superset);
    }
  }

  // Move account profiles to the front of the vector.
  std::ranges::partition(profiles,
                         std::mem_fn(&AutofillProfile::IsAccountProfile));

  // Deduplicate mergeable profiles. Merging always to the latter profile is
  // safe because:
  // 1. If the record type is the same, it doesn't matter.
  // 2. If the record type is different, the local profile will be latter.
  for (auto profile_it = profiles.begin(); profile_it != profiles.end();
       ++profile_it) {
    if (guids_to_delete.contains(profile_it->guid())) {
      continue;
    }
    // If possible, merge `*profile_it` with another profile and remove it.
    if (auto merge_candidate = std::find_if(
            profile_it + 1, profiles.end(),
            [&](const AutofillProfile& other_profile) {
              return !guids_to_delete.contains(other_profile.guid()) &&
                     comparator.AreMergeable(*profile_it, other_profile);
            });
        merge_candidate != profiles.end()) {
      merge_candidate->MergeDataFrom(*profile_it, comparator.app_locale());
      adm.UpdateProfile(*merge_candidate);
      guids_to_delete.insert(profile_it->guid());
    }
  }
  for (const std::string& guid : guids_to_delete) {
    adm.RemoveProfile(guid, /*non_permanent_account_profile_removal=*/true);
  }
  autofill_metrics::LogNumberOfProfilesRemovedDuringDedupe(
      guids_to_delete.size());
}

template <typename T, typename Proj>
std::vector<T> CalculateMinimalIncompatibleTypeSetsImpl(
    const AutofillProfile& profile,
    base::span<const AutofillProfile* const> other_profiles,
    const AutofillProfileComparator& comparator,
    Proj proj) {
  std::vector<T> min_incompatible_sets;
  size_t current_minimum = SIZE_MAX;
  for (const AutofillProfile* other : other_profiles) {
    if (profile.guid() == other->guid()) {
      // When computing `CalculateMinimalIncompatibleTypeSets()` for every
      // profile in a list of profiles, it's convenient to call the function
      // with that list as `other_profiles`. Skip the `profile` entry.
      continue;
    }
    const std::optional<FieldTypeSet> differing_types =
        comparator.NonMergeableSettingVisibleTypes(profile, *other);
    if (!differing_types) {
      continue;
    }

    // Replace `min_incompatible_sets` if `differing_types->size()`
    // is a new minimum or add to it, if it matches the current minimum.
    if (differing_types->size() < current_minimum) {
      current_minimum = differing_types->size();
      min_incompatible_sets.clear();
    }
    if (differing_types->size() == current_minimum) {
      min_incompatible_sets.push_back(proj(other, *differing_types));
    }
  }
  return min_incompatible_sets;
}

}  // namespace

AddressDataCleaner::AddressDataCleaner(
    AddressDataManager& address_data_manager,
    syncer::SyncService* sync_service,
    PrefService& pref_service,
    AlternativeStateNameMapUpdater* alternative_state_name_map_updater)
    : address_data_manager_(address_data_manager),
      sync_service_(sync_service),
      pref_service_(pref_service),
      alternative_state_name_map_updater_(alternative_state_name_map_updater) {
  adm_observer_.Observe(&address_data_manager_.get());
  if (sync_service_) {
    sync_observer_.Observe(sync_service_);
  }
}

AddressDataCleaner::~AddressDataCleaner() = default;

void AddressDataCleaner::MaybeCleanupAddressData() {
  if (!are_cleanups_pending_ || ShouldWaitForSync(sync_service_)) {
    return;
  }
  are_cleanups_pending_ = false;

  int chrome_version_major = version_info::GetMajorVersionNumberAsInt();
  // Ensure that deduplication is only run once per milestone, unless it is
  // explicitly always enabled.
  if (pref_service_->GetInteger(prefs::kAutofillLastVersionDeduped) <
          chrome_version_major ||
      base::FeatureList::IsEnabled(
          features::debug::kAutofillSkipDeduplicationRequirements)) {
    pref_service_->SetInteger(prefs::kAutofillLastVersionDeduped,
                              chrome_version_major);
    // Since the milestone changed the extra deduplication can be run again.
    pref_service_->ClearPref(
        prefs::kAutofillRanExtraDeduplication);
    ApplyDeduplicationRoutine();
  } else if (base::FeatureList::IsEnabled(
                 features::kAutofillDeduplicateAccountAddresses) &&
             !pref_service_->GetBoolean(
                 prefs::kAutofillRanExtraDeduplication)) {
    pref_service_->SetBoolean(
        prefs::kAutofillRanExtraDeduplication, true);
    ApplyDeduplicationRoutine();
  }

  // Other cleanups are performed on every browser start.
  MigratePhoneticNames();
  DeleteDisusedAddresses();
}

// static
std::vector<DifferingProfileWithTypeSet>
AddressDataCleaner::CalculateMinimalIncompatibleProfileWithTypeSets(
    const AutofillProfile& profile,
    base::span<const AutofillProfile* const> existing_profiles,
    const AutofillProfileComparator& comparator) {
  return CalculateMinimalIncompatibleTypeSetsImpl<DifferingProfileWithTypeSet>(
      profile, existing_profiles, comparator,
      [](const AutofillProfile* other, FieldTypeSet s) {
        return DifferingProfileWithTypeSet(other, s);
      });
}

void AddressDataCleaner::ApplyDeduplicationRoutine() {
  // Since deduplication (more specifically, comparing profiles) depends on the
  // `AlternativeStateNameMap`, make sure that it gets populated first.
  if (alternative_state_name_map_updater_ &&
      !alternative_state_name_map_updater_
           ->is_alternative_state_name_map_populated()) {
    alternative_state_name_map_updater_->PopulateAlternativeStateNameMap(
        base::BindOnce(&AddressDataCleaner::ApplyDeduplicationRoutine,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  const std::vector<const AutofillProfile*>& profiles =
      address_data_manager_->GetProfiles(
          AddressDataManager::ProfileOrder::kHighestFrecencyDesc);
  // Early return to prevent polluting metrics with uninteresting events.
  if (profiles.size() < 2) {
    return;
  }
  autofill_metrics::LogNumberOfProfilesConsideredForDedupe(profiles.size());

  // `profiles` contains pointers to the PDM's state. Modifying them directly
  // won't update them in the database and calling `PDM:UpdateProfile()`
  // would discard them as a duplicate.
  std::vector<AutofillProfile> deduplicated_profiles;
  for (const AutofillProfile* profile : profiles) {
    deduplicated_profiles.push_back(*profile);
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillDeduplicateAccountAddresses)) {
    DeduplicateWithAccountProfiles(
        AutofillProfileComparator(address_data_manager_->app_locale()),
        std::move(deduplicated_profiles), *address_data_manager_);
  } else {
  DeduplicateProfiles(
      AutofillProfileComparator(address_data_manager_->app_locale()),
      std::move(deduplicated_profiles), *address_data_manager_);
  }
}

void AddressDataCleaner::MigratePhoneticNames() {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    return;
  }
  int migrated_names = 0;
  for (const AutofillProfile* profile : address_data_manager_->GetProfiles()) {
    if (profile->GetNameInfo().HasNameEligibleForPhoneticNameMigration()) {
      AutofillProfile profile_to_migrate = *profile;
      profile_to_migrate.MigrateRegularNameToPhoneticName();
      address_data_manager_->UpdateProfile(profile_to_migrate);
      migrated_names++;
    }
  }
  autofill_metrics::LogNumberOfNamesMigratedDuringCleanup(migrated_names);
}

void AddressDataCleaner::DeleteDisusedAddresses() {
  std::vector<const AutofillProfile*> profiles =
      base::FeatureList::IsEnabled(
          features::kAutofillDeduplicateAccountAddresses)
          ? address_data_manager_->GetProfiles()
          : address_data_manager_->GetProfilesByRecordType(
                AutofillProfile::RecordType::kLocalOrSyncable);
  // Early return to prevent polluting metrics with uninteresting events.
  if (profiles.empty()) {
    return;
  }
  // Don't call `PDM::RemoveByGUID()` directly, since this can invalidate the
  // pointers in `profiles`.
  std::vector<std::string> guids_to_delete;
  for (const AutofillProfile* profile : profiles) {
    if (IsAutofillEntryWithUseDateDeletable(
            profile->usage_history().use_date())) {
      guids_to_delete.push_back(profile->guid());
    }
  }
  for (const std::string& guid : guids_to_delete) {
    address_data_manager_->RemoveProfile(
        guid,
        /*non_permanent_account_profile_removal=*/true);
  }
  autofill_metrics::LogNumberOfAddressesDeletedForDisuse(
      guids_to_delete.size());
}

void AddressDataCleaner::OnAddressDataChanged() {
  MaybeCleanupAddressData();
}

void AddressDataCleaner::OnStateChanged(syncer::SyncService* sync_service) {
  // After sync has started, it's possible that the ADM is still reloading any
  // changed data from the database. In this case, delay the cleanups slightly
  // longer until `OnAddressDataChanged()` is called.
  if (!address_data_manager_->IsAwaitingPendingAddressChanges()) {
    MaybeCleanupAddressData();
  }
}

void AddressDataCleaner::OnSyncShutdown(syncer::SyncService*) {
  // Unreachable, since the service owning this instance is Shutdown() before
  // the SyncService.
  NOTREACHED();
}

}  // namespace autofill
