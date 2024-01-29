// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_data_cleaner.h"

#include <set>

#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
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

// Goes through all the `existing_profiles` and merges all similar profiles
// together. All the profiles except the results of the merges will be added to
// `profile_guids_to_delete`.
// Comparisons are done using `comparator`.
void DeduplicateProfiles(
    const AutofillProfileComparator comparator,
    std::vector<std::unique_ptr<AutofillProfile>>& existing_profiles,
    std::set<std::string>& profiles_to_delete) {
  AutofillMetrics::LogNumberOfProfilesConsideredForDedupe(
      existing_profiles.size());

  // Sort the profiles by ranking score. That way the most relevant profiles
  // will get merged into the less relevant profiles, which keeps the syntax of
  // the most relevant profiles data.
  // Since profiles earlier in the list are merged into profiles later in the
  // list, `kLocalOrSyncable` profiles are placed before `kAccount` profiles.
  // This is because local profiles can be merged into account profiles, but not
  // the other way around.
  // TODO(crbug.com/1411114): Remove code duplication for sorting profiles.
  base::ranges::sort(
      existing_profiles, [comparison_time = AutofillClock::Now()](
                             const std::unique_ptr<AutofillProfile>& a,
                             const std::unique_ptr<AutofillProfile>& b) {
        if (a->source() != b->source()) {
          return a->source() == AutofillProfile::Source::kLocalOrSyncable;
        }
        return a->HasGreaterRankingThan(b.get(), comparison_time);
      });

  for (auto i = existing_profiles.begin(); i != existing_profiles.end(); ++i) {
    AutofillProfile* profile_to_merge = i->get();

    // If the profile was set to be deleted, skip it. This can happen because
    // the loop below reassigns `profile_to_merge` to (effectively) `j->get()`.
    if (profiles_to_delete.contains(profile_to_merge->guid())) {
      continue;
    }

    // Profiles in the account storage should not be silently deleted.
    if (profile_to_merge->source() == AutofillProfile::Source::kAccount) {
      continue;
    }

    // Try to merge `profile_to_merge` with a less relevant `existing_profiles`.
    for (auto j = i + 1; j < existing_profiles.end(); ++j) {
      AutofillProfile& existing_profile = **j;

      // Don't try to merge a profile that was already set for deletion or that
      // cannot be merged.
      if (profiles_to_delete.contains(existing_profile.guid()) ||
          !comparator.AreMergeable(existing_profile, *profile_to_merge)) {
        continue;
      }

      // No new information should silently be introduced to the account
      // storage. So for account profiles, only merge if the `kLocalOrSyncable`
      // `profile_to_merge` is a subset.
      if (existing_profile.source() == AutofillProfile::Source::kAccount &&
          !profile_to_merge->IsSubsetOf(comparator, existing_profile)) {
        continue;
      }

      // The profiles are found to be mergeable; update the existing profile.
      existing_profile.SaveAdditionalInfo(*profile_to_merge,
                                          comparator.app_locale());
      profiles_to_delete.insert(profile_to_merge->guid());

      // Account profiles track from which service they originate. This allows
      // Autofill to distinguish between Chrome and non-Chrome account
      // profiles and measure the added utility of non-Chrome profiles. Since
      // the `existing_profile` matched the information that was already
      // present in Autofill (`profile_to_merge`), the account profile doesn't
      // provide any utility. To capture this in the metric, the merged
      // profile is treated as a Chrome account profile.
      if (existing_profile.source() == AutofillProfile::Source::kAccount) {
        existing_profile.set_initial_creator_id(
            AutofillProfile::kInitialCreatorOrModifierChrome);
        existing_profile.set_last_modifier_id(
            AutofillProfile::kInitialCreatorOrModifierChrome);
      }

      // Now try to merge the new resulting profile with the rest of the
      // existing profiles.
      profile_to_merge = &existing_profile;
      // Account profiles cannot be merged into other profiles, since that
      // would delete them. Note that the `existing_profile` (now
      // `profile_to_merge`) might be verified.
      if (profile_to_merge->source() == AutofillProfile::Source::kAccount) {
        break;
      }
    }
  }
  AutofillMetrics::LogNumberOfProfilesRemovedDuringDedupe(
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
      personal_data_manager_->GetProfiles();
  // Early return to prevent polluting metrics with uninteresting events.
  if (profiles.size() < 2) {
    return;
  }

  // `profiles` contains pointers to the PDM's state. Modifying them directly
  // won't update them in the database and calling `PDM:UpdateProfile()`
  // would discard them as a duplicate.
  std::vector<std::unique_ptr<AutofillProfile>> new_profiles;
  for (const AutofillProfile* profile : profiles) {
    new_profiles.push_back(std::make_unique<AutofillProfile>(*profile));
  }
  std::set<std::string> profiles_to_delete;
  DeduplicateProfiles(
      AutofillProfileComparator(personal_data_manager_->app_locale()),
      new_profiles, profiles_to_delete);

  // Apply the profile changes to the database.
  for (const std::unique_ptr<AutofillProfile>& profile : new_profiles) {
    if (profiles_to_delete.contains(profile->guid())) {
      personal_data_manager_->RemoveByGUID(profile->guid());
    } else {
      personal_data_manager_->UpdateProfile(*profile);
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
  AutofillMetrics::LogNumberOfAddressesDeletedForDisuse(guids_to_delete.size());
}

void AddressDataCleaner::OnPersonalDataFinishedProfileTasks() {
  MaybeCleanupAddressData();
}

void AddressDataCleaner::OnStateChanged(syncer::SyncService* sync_service) {
  // After sync has started, it's possible that the PDM is still reloading any
  // changed data from the database. In this case, delay the cleanups slightly
  // longer until `OnPersonalDataFinishedProfileTasks()` is called.
  if (!personal_data_manager_->IsAwaitingPendingChanges()) {
    MaybeCleanupAddressData();
  }
}

}  // namespace autofill
