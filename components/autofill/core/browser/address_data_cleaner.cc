// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_data_cleaner.h"

#include "base/containers/to_vector.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/address_data_cleaner_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/profile_token_quality.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
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
void DeduplicateProfiles(const AutofillProfileComparator& comparator,
                         std::vector<AutofillProfile> profiles,
                         AddressDataManager& adm) {
  // Partition the profiles into local and account profiles:
  // - Local: [profiles.begin(), bgn_account_profiles[
  // - Account: [bgn_account_profiles, profiles.end()[
  auto bgn_account_profiles = base::ranges::stable_partition(
      profiles, std::not_fn(&AutofillProfile::IsAccountProfile));

  size_t num_profiles_deleted = 0;
  for (auto local_profile_it = profiles.begin();
       local_profile_it != bgn_account_profiles; ++local_profile_it) {
    // If possible, merge `*local_profile_it` with another local profile and
    // remove it.
    if (auto merge_candidate = base::ranges::find_if(
            local_profile_it + 1, bgn_account_profiles,
            [&](const AutofillProfile& local_profile2) {
              return comparator.AreMergeable(*local_profile_it, local_profile2);
            });
        merge_candidate != bgn_account_profiles) {
      merge_candidate->MergeDataFrom(*local_profile_it,
                                     comparator.app_locale());
      adm.UpdateProfile(*merge_candidate);
      adm.RemoveProfile(local_profile_it->guid());
      num_profiles_deleted++;
      continue;
    }
    // `*local_profile_it` is not mergeable with another local profile. But it
    // might be a subset of an account profile and can thus be removed.
    if (auto superset_account_profile = base::ranges::find_if(
            bgn_account_profiles, profiles.end(),
            [&](const AutofillProfile& account_profile) {
              return comparator.AreMergeable(*local_profile_it,
                                             account_profile) &&
                     local_profile_it->IsSubsetOf(comparator, account_profile);
            });
        superset_account_profile != profiles.end()) {
      adm.RemoveProfile(local_profile_it->guid());
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

  // Ensure that deduplication is only run one per milestone.
  if (pref_service_->GetInteger(prefs::kAutofillLastVersionDeduped) <
      CHROME_VERSION_MAJOR) {
    pref_service_->SetInteger(prefs::kAutofillLastVersionDeduped,
                              CHROME_VERSION_MAJOR);
    ApplyDeduplicationRoutine();
  }

  // Other cleanups are performed on every browser start.
  DeleteDisusedAddresses();
}

// static
std::vector<FieldTypeSet>
AddressDataCleaner::CalculateMinimalIncompatibleTypeSets(
    const AutofillProfile& profile,
    base::span<const AutofillProfile> other_profiles,
    const AutofillProfileComparator& comparator) {
  return CalculateMinimalIncompatibleTypeSetsImpl<FieldTypeSet>(
      profile, base::ToVector(other_profiles, [](auto& x) { return &x; }),
      comparator, [](const AutofillProfile*, FieldTypeSet s) { return s; });
}

// static
std::vector<FieldTypeSet>
AddressDataCleaner::CalculateMinimalIncompatibleTypeSets(
    const AutofillProfile& profile,
    base::span<const AutofillProfile* const> existing_profiles,
    const AutofillProfileComparator& comparator) {
  return CalculateMinimalIncompatibleTypeSetsImpl<FieldTypeSet>(
      profile, existing_profiles, comparator,
      [](const AutofillProfile*, FieldTypeSet s) { return s; });
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

// static
bool AddressDataCleaner::IsTokenLowQualityForDeduplicationPurposes(
    const AutofillProfile& profile,
    FieldType type) {
  // A token is considered low quality for deduplication purposes, if the
  // majority of its observers are "bad", as defined by the switch below.
  auto [count_good, count_bad] =
      CountObservationsByQualityForDeduplicationPurposes(
          profile.token_quality().GetObservationTypesForFieldType(type));
  return count_good + count_bad >= 4 && count_bad - count_good >= 2;
}

// static
std::pair<size_t, size_t>
AddressDataCleaner::CountObservationsByQualityForDeduplicationPurposes(
    base::span<const ProfileTokenQuality::ObservationType> observations) {
  using ObservationType = ProfileTokenQuality::ObservationType;
  size_t count_good = 0, count_bad = 0;
  for (ObservationType observation : observations) {
    switch (observation) {
      case ObservationType::kAccepted:
        count_good++;
        break;
      case ObservationType::kEditedToSimilarValue:
      case ObservationType::kEditedToDifferentTokenOfSameProfile:
      case ObservationType::kEditedToSameTokenOfOtherProfile:
      case ObservationType::kEditedToDifferentTokenOfOtherProfile:
      case ObservationType::kEditedFallback:
        count_bad++;
        break;
      case ObservationType::kUnknown:
      case ObservationType::kPartiallyAccepted:
      case ObservationType::kEditedValueCleared:
        // These observation types are considered neutral. They are irrelevant
        // for deduplication purposes.
        break;
    }
  }
  return {count_good, count_bad};
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
  DeduplicateProfiles(
      AutofillProfileComparator(address_data_manager_->app_locale()),
      std::move(deduplicated_profiles), *address_data_manager_);
}

void AddressDataCleaner::DeleteDisusedAddresses() {
  const std::vector<const AutofillProfile*>& profiles =
      address_data_manager_->GetProfilesByRecordType(
          AutofillProfile::RecordType::kLocalOrSyncable);
  // Early return to prevent polluting metrics with uninteresting events.
  if (profiles.empty()) {
    return;
  }
  // Don't call `PDM::RemoveByGUID()` directly, since this can invalidate the
  // pointers in `profiles`.
  std::vector<std::string> guids_to_delete;
  for (const AutofillProfile* profile : profiles) {
    if (IsAutofillEntryWithUseDateDeletable(profile->use_date())) {
      guids_to_delete.push_back(profile->guid());
    }
  }
  for (const std::string& guid : guids_to_delete) {
    address_data_manager_->RemoveProfile(guid);
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

}  // namespace autofill
