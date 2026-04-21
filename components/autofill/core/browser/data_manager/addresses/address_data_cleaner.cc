// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/addresses/address_data_cleaner.h"

#include <algorithm>

#include "base/containers/to_vector.h"
#include "base/metrics/histogram_macros.h"
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

using ProfileAction = AddressDataCleaner::ProfileAction;
using ProfileWithAction = AddressDataCleaner::ProfileWithAction;

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

// Merges mergeable profiles in `profiles`. This supports both local and account
// profiles and preserves the `initial_creator_id`. The algorithm proceeds in
// two steps, such that the amount of retained information is maximized without
// sending the data to the account if it was stored locally. Note that due to
// normalisation, etc, even if `IsSubsetOf()` is true, the information present
// in the subset can still look slightly different from the superset and is
// therefore not silently merged.
// 1) Marks all profiles that are subsets of another profile for removal.
//    If a profile is a subset of multiple other profiles, its usage history is
//    merged with all of them and they are marked for update. The silent updates
//    that the subset may have contained are intentionally dropped, such that
//    this information is not uploaded to the account without consent. For exact
//    duplicates, keeping the account profile is preferred.
// 2) Merges pairs of mergeable profiles into each other.
//    To prevent silently introducing new information into the account,
//    local profiles are never merged into account profiles. The subsumed
//    profile is marked for removal and the merged profile is marked for update.
void DeduplicateProfiles(const AutofillProfileComparator& comparator,
                         std::vector<ProfileWithAction>& profiles_with_action) {
  SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.DeduplicateProfiles");
  size_t removed_profiles_count = 0;
  for (auto& [profile, profile_action] : profiles_with_action) {
    // Returns true if `profile` is a subset of `superset`. Note that
    // `is_subset(A, A)` returns false.
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

    for (auto& [superset, superset_action] : profiles_with_action) {
      if (superset_action == ProfileAction::kRemove || !is_subset(superset)) {
        continue;
      }

      ++removed_profiles_count;
      profile_action = ProfileAction::kRemove;
      superset_action = ProfileAction::kUpdate;
      superset.usage_history().MergeUsageHistories(profile.usage_history());
    }
  }

  // Move account profiles to the front of the vector.
  std::ranges::partition(
      profiles_with_action, [](const ProfileWithAction& profile_with_action) {
        return profile_with_action.profile.IsAccountProfile();
      });

  // Deduplicate mergeable profiles. Merging always to the latter profile is
  // safe because:
  // 1. If the record type is the same, it doesn't matter.
  // 2. If the record type is different, the local profile will be latter.
  //    This ensures that account profiles are merged into local profiles,
  //    retaining the combined information locally and preventing the silent
  //    upload of local data to the user's Google account without explicit
  //    consent.
  for (auto profile_it = profiles_with_action.begin();
       profile_it != profiles_with_action.end(); ++profile_it) {
    if (profile_it->action == ProfileAction::kRemove) {
      continue;
    }
    // If possible, merge `*profile_it` with another profile and remove it.
    if (auto merge_candidate = std::find_if(
            profile_it + 1, profiles_with_action.end(),
            [&](const ProfileWithAction& other_profile) {
              return other_profile.action != ProfileAction::kRemove &&
                     comparator.AreMergeable(profile_it->profile,
                                             other_profile.profile);
            });
        merge_candidate != profiles_with_action.end()) {
      merge_candidate->profile.MergeDataFrom(profile_it->profile,
                                             comparator.app_locale());
      profile_it->action = ProfileAction::kRemove;
      merge_candidate->action = ProfileAction::kUpdate;
      ++removed_profiles_count;
    }
  }

  autofill_metrics::LogNumberOfProfilesRemovedDuringDedupe(
      removed_profiles_count);
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

void AddressDataCleaner::ApplyProfileActions(
    const std::vector<ProfileWithAction>& profiles_with_action) {
  for (const auto& [profile, action] : profiles_with_action) {
    switch (action) {
      case ProfileAction::kUpdate:
        address_data_manager_->UpdateProfile(profile);
        break;
      case ProfileAction::kRemove:
        address_data_manager_->RemoveProfile(
            profile.guid(), /*non_permanent_account_profile_removal=*/true);
        break;
      case ProfileAction::kNone:
        break;
    }
  }
}

void AddressDataCleaner::MaybeCleanupAddressData() {
  if (!are_cleanups_pending_ || ShouldWaitForSync(sync_service_)) {
    return;
  }

  // Since deduplication (more specifically, comparing profiles) depends on the
  // `AlternativeStateNameMap`, make sure that it gets populated first.
  if (alternative_state_name_map_updater_ &&
      !alternative_state_name_map_updater_
           ->is_alternative_state_name_map_populated()) {
    alternative_state_name_map_updater_->PopulateAlternativeStateNameMap(
        base::BindOnce(&AddressDataCleaner::MaybeCleanupAddressData,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  are_cleanups_pending_ = false;

  // Accumulates the local changes applied to profiles during cleanup routines
  // (e.g. deduplication or phonetic name migration) and tracks the pending
  // AddressDataManager operation for each profile. Modifying them directly
  // won't update them in the database and calling `PDM:UpdateProfile()` would
  // discard them as a duplicate.
  std::vector<ProfileWithAction> profiles_with_action = base::ToVector(
      address_data_manager_->GetProfiles(
          AddressDataManager::ProfileOrder::kHighestFrecencyDesc),
      [](const AutofillProfile* profile) {
        return ProfileWithAction{*profile};
      });

  // Disused profiles are marked for cleanup on every browser start.
  MarkDisusedProfilesForDeletion(profiles_with_action);

  // Profiles are marked for phonetic name migration on every browser start.
  MarkProfilesForPhoneticNameMigration(profiles_with_action);

  // Ensure that deduplication is only run once per milestone, unless it is
  // explicitly always enabled.
  if (pref_service_->GetInteger(prefs::kAutofillLastVersionDeduped) <
          version_info::GetMajorVersionNumberAsInt() ||
      base::FeatureList::IsEnabled(
          features::debug::kAutofillSkipDeduplicationRequirements)) {
    pref_service_->SetInteger(prefs::kAutofillLastVersionDeduped,
                              version_info::GetMajorVersionNumberAsInt());
    ApplyDeduplicationRoutine(profiles_with_action);
  }

  // Apply profiles action via the address data manager.
  ApplyProfileActions(profiles_with_action);
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

void AddressDataCleaner::ApplyDeduplicationRoutine(
    std::vector<ProfileWithAction>& profiles_with_action) {
  const size_t profiles_to_dedup_count =
      profiles_with_action.size() -
      std::ranges::count(profiles_with_action, ProfileAction::kRemove,
                         &ProfileWithAction::action);

  // Early return to prevent polluting metrics with uninteresting events.
  if (profiles_to_dedup_count < 2) {
    return;
  }

  autofill_metrics::LogNumberOfProfilesConsideredForDedupe(
      profiles_to_dedup_count);
  autofill_metrics::LogNumberOfProfilesConsideredForDedupePerCountryCode(
      profiles_with_action);

  DeduplicateProfiles(
      AutofillProfileComparator(address_data_manager_->app_locale()),
      profiles_with_action);
}

void AddressDataCleaner::MarkProfilesForPhoneticNameMigration(
    std::vector<ProfileWithAction>& profiles_with_action) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    return;
  }
  size_t migrated_names = 0;
  for (auto& [profile, action] : profiles_with_action) {
    if (action != ProfileAction::kRemove &&
        profile.GetNameInfo().HasNameEligibleForPhoneticNameMigration()) {
      profile.MigrateRegularNameToPhoneticName();
      action = ProfileAction::kUpdate;
      ++migrated_names;
    }
  }
  autofill_metrics::LogNumberOfNamesMigratedDuringCleanup(migrated_names);
}

void AddressDataCleaner::MarkDisusedProfilesForDeletion(
    std::vector<ProfileWithAction>& profiles_with_action) {
  // Early return to prevent polluting metrics with uninteresting events.
  if (profiles_with_action.empty()) {
    return;
  }
  // Don't call `PDM::RemoveByGUID()` directly, since this can invalidate the
  // pointers in `profiles`.
  size_t disused_profiles_count = 0;
  for (auto& [profile, action] : profiles_with_action) {
    if (IsAutofillEntryWithUseDateDeletable(
            profile.usage_history().use_date())) {
      action = ProfileAction::kRemove;
      ++disused_profiles_count;
    }
  }
  autofill_metrics::LogNumberOfAddressesDeletedForDisuse(
      disused_profiles_count);
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
