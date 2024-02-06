// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_data_cleaner.h"

#include <set>

#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/metrics/address_data_cleaner_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace autofill {

namespace {

// Determines whether cleanups should be deferred because the latest data wasn't
// synced down yet.
bool ShouldWaitForSync(syncer::SyncService* sync_service) {
  // No need to wait if the user is not syncing addresses.
  if (!sync_service || !sync_service->GetUserSettings()->GetSelectedTypes().Has(
                           syncer::UserSelectableType::kAutofill)) {
    return false;
  }

  auto should_wait = [&sync_service](syncer::ModelType model_type) {
    switch (sync_service->GetDownloadStatusFor(model_type)) {
      case syncer::SyncService::ModelTypeDownloadStatus::kWaitingForUpdates:
        return true;
      case syncer::SyncService::ModelTypeDownloadStatus::kUpToDate:
      // If the download status is kError, it will likely not become available
      // anytime soon. In this case, don't defer the cleanups.
      case syncer::SyncService::ModelTypeDownloadStatus::kError:
        return false;
    }
    NOTREACHED_NORETURN();
  };
  return should_wait(syncer::ModelType::AUTOFILL_PROFILE) ||
         should_wait(syncer::ModelType::CONTACT_INFO);
}

// - Merges local profiles occurring earlier in `profiles` with mergeable other
//   local profiles later in `profiles`, marking the earlier one as deletable.
// - Marks local profiles that are subsets of account profiles as deletable.
// Mergability is determined using `comparator`.
// The elements of `profiles` are updated and potentially reordered.
void DeduplicateProfiles(const AutofillProfileComparator& comparator,
                         std::vector<AutofillProfile>& profiles,
                         std::set<std::string>& profiles_to_delete) {
  autofill_metrics::LogNumberOfProfilesConsideredForDedupe(profiles.size());

  // Partition the profiles into local and account profiles:
  // - Local: [profiles.begin(), bgn_account_profiles[
  // - Account: [bgn_account_profiles, profiles.end()[
  auto bgn_account_profiles =
      base::ranges::stable_partition(profiles, [](const AutofillProfile& p) {
        return p.source() == AutofillProfile::Source::kLocalOrSyncable;
      });

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
      profiles_to_delete.insert(local_profile_it->guid());
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
      profiles_to_delete.insert(local_profile_it->guid());
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
    }
  }
  autofill_metrics::LogNumberOfProfilesRemovedDuringDedupe(
      profiles_to_delete.size());
}

}  // namespace

AddressDataCleaner::AddressDataCleaner(
    PersonalDataManager& personal_data_manager,
    syncer::SyncService* sync_service,
    PrefService& pref_service,
    AlternativeStateNameMapUpdater* alternative_state_name_map_updater)
    : personal_data_manager_(personal_data_manager),
      sync_service_(sync_service),
      pref_service_(pref_service),
      alternative_state_name_map_updater_(alternative_state_name_map_updater) {
  pdm_observer_.Observe(&personal_data_manager_.get());
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

  const std::vector<AutofillProfile*>& profiles =
      personal_data_manager_->GetProfiles(
          PersonalDataManager::ProfileOrder::kHighestFrecencyDesc);
  // Early return to prevent polluting metrics with uninteresting events.
  if (profiles.size() < 2) {
    return;
  }

  // `profiles` contains pointers to the PDM's state. Modifying them directly
  // won't update them in the database and calling `PDM:UpdateProfile()`
  // would discard them as a duplicate.
  std::vector<AutofillProfile> deduplicated_profiles;
  for (const AutofillProfile* profile : profiles) {
    deduplicated_profiles.push_back(*profile);
  }
  std::set<std::string> profiles_to_delete;
  DeduplicateProfiles(
      AutofillProfileComparator(personal_data_manager_->app_locale()),
      deduplicated_profiles, profiles_to_delete);

  // Apply the profile changes to the database.
  for (const AutofillProfile& profile : deduplicated_profiles) {
    if (profiles_to_delete.contains(profile.guid())) {
      personal_data_manager_->RemoveByGUID(profile.guid());
    } else {
      personal_data_manager_->UpdateProfile(profile);
    }
  }
}

void AddressDataCleaner::DeleteDisusedAddresses() {
  const std::vector<AutofillProfile*>& profiles =
      personal_data_manager_->GetProfilesFromSource(
          AutofillProfile::Source::kLocalOrSyncable);
  // Early return to prevent polluting metrics with uninteresting events.
  if (profiles.empty()) {
    return;
  }
  // Don't call `PDM::RemoveByGUID()` directly, since this can invalidate the
  // pointers in `profiles`.
  std::vector<std::string> guids_to_delete;
  for (AutofillProfile* profile : profiles) {
    if (profile->IsDeletable()) {
      guids_to_delete.push_back(profile->guid());
    }
  }
  for (const std::string& guid : guids_to_delete) {
    personal_data_manager_->RemoveByGUID(guid);
  }
  autofill_metrics::LogNumberOfAddressesDeletedForDisuse(
      guids_to_delete.size());
}

void AddressDataCleaner::OnPersonalDataFinishedProfileTasks() {
  MaybeCleanupAddressData();
}

void AddressDataCleaner::OnStateChanged(syncer::SyncService* sync_service) {
  // After sync has started, it's possible that the PDM is still reloading any
  // changed data from the database. In this case, delay the cleanups slightly
  // longer until `OnPersonalDataFinishedProfileTasks()` is called.
  if (!personal_data_manager_->IsAwaitingPendingAddressChanges()) {
    MaybeCleanupAddressData();
  }
}

}  // namespace autofill
