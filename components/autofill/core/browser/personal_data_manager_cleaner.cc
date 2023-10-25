// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager_cleaner.h"

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"

namespace autofill {

PersonalDataManagerCleaner::PersonalDataManagerCleaner(
    PersonalDataManager* personal_data_manager,
    AlternativeStateNameMapUpdater* alternative_state_name_map_updater,
    PrefService* pref_service)
    : personal_data_manager_(personal_data_manager),
      pref_service_(pref_service),
      alternative_state_name_map_updater_(alternative_state_name_map_updater) {
  // Check if profile cleanup has already been performed this major version.
  is_autofill_profile_dedupe_pending_ =
      pref_service_->GetInteger(prefs::kAutofillLastVersionDeduped) <
      CHROME_VERSION_MAJOR;
}

PersonalDataManagerCleaner::~PersonalDataManagerCleaner() = default;

void PersonalDataManagerCleaner::CleanupDataAndNotifyPersonalDataObservers() {
  // The profile de-duplication is run once every major chrome version. If the
  // profile de-duplication has not run for the |CHROME_VERSION_MAJOR| yet,
  // |AlternativeStateNameMap| needs to be populated first. Otherwise,
  // defer the insertion to when the observers are notified.
  if (!alternative_state_name_map_updater_
           ->is_alternative_state_name_map_populated() &&
      is_autofill_profile_dedupe_pending_) {
    alternative_state_name_map_updater_->PopulateAlternativeStateNameMap(
        base::BindOnce(&PersonalDataManagerCleaner::
                           CleanupDataAndNotifyPersonalDataObservers,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // If the user has chosen to sync addresses, wait for sync to start before
  // performing cleanups. Otherwise do them now.
  if (!personal_data_manager_->IsUserSelectableTypeEnabled(
          syncer::UserSelectableType::kAutofill)) {
    ApplyAddressFixesAndCleanups();
  }
  // If the user has chosen to sync payments, wait for sync to start before
  // performing cleanups. Otherwise do them now.
  if (!personal_data_manager_->IsUserSelectableTypeEnabled(
          syncer::UserSelectableType::kPayments)) {
    ApplyCardFixesAndCleanups();
  }

  // Log address, credit card, offer, IBAN, and usage data startup metrics.
  personal_data_manager_->LogStoredDataMetrics();

  personal_data_manager_->NotifyPersonalDataObserver();
}

void PersonalDataManagerCleaner::SyncStarted(syncer::ModelType model_type) {}

void PersonalDataManagerCleaner::ApplyAddressAndCardFixesAndCleanups(
    syncer::ModelType model_type) {
  // The profile de-duplication is run once every major chrome version. If the
  // profile de-duplication has not run for the |CHROME_VERSION_MAJOR| yet,
  // |AlternativeStateNameMap| needs to be populated first. Otherwise,
  // defer the insertion to when the observers are notified.
  // TODO(crbug.com/1111960): If sync is disabled and re-enabled, the
  // alternative state name map should be re-populated. This is currently not
  // the case due to the `is_alternative_state_name_map_populated()` check. This
  // state should be reset when sync is disabled, together with
  // `autofill_profile_sync_started_` and `contact_info_sync_started_`.
  autofill_profile_sync_started_ |= model_type == syncer::AUTOFILL_PROFILE;
  contact_info_sync_started_ |= model_type == syncer::CONTACT_INFO;
  if (autofill_profile_sync_started_ && contact_info_sync_started_ &&
      !alternative_state_name_map_updater_
           ->is_alternative_state_name_map_populated() &&
      is_autofill_profile_dedupe_pending_) {
    alternative_state_name_map_updater_->PopulateAlternativeStateNameMap(
        base::BindOnce(
            &PersonalDataManagerCleaner::ApplyAddressAndCardFixesAndCleanups,
            weak_ptr_factory_.GetWeakPtr(), model_type));
    return;
  }

  // Run deferred autofill address profile startup code.
  if (autofill_profile_sync_started_ && contact_info_sync_started_ &&
      (model_type == syncer::AUTOFILL_PROFILE ||
       model_type == syncer::CONTACT_INFO)) {
    ApplyAddressFixesAndCleanups();
  }

  // Run deferred credit card startup code.
  if (model_type == syncer::AUTOFILL_WALLET_DATA) {
    ApplyCardFixesAndCleanups();
  }
}

void PersonalDataManagerCleaner::ApplyAddressFixesAndCleanups() {
  if (!is_profile_cleanup_pending_) {
    return;
  }

  // Once per major version, otherwise NOP.
  ApplyDedupingRoutine();

  // Ran once on browser startup.
  DeleteDisusedAddresses();

  // Ran once on browser startup.
  RemoveInaccessibleProfileValues();

  is_profile_cleanup_pending_ = false;
}

void PersonalDataManagerCleaner::ApplyCardFixesAndCleanups() {
  if (!is_credit_card_cleanup_pending_) {
    return;
  }

  // Ran once on browser startup.
  DeleteDisusedCreditCards();

  // Ran once on browser startup.
  ClearCreditCardNonSettingsOrigins();

  is_credit_card_cleanup_pending_ = true;
}

void PersonalDataManagerCleaner::RemoveInaccessibleProfileValues() {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillRemoveInaccessibleProfileValuesOnStartup)) {
    return;
  }

  for (const AutofillProfile* profile :
       personal_data_manager_->GetProfilesFromSource(
           AutofillProfile::Source::kLocalOrSyncable)) {
    const ServerFieldTypeSet inaccessible_fields =
        profile->FindInaccessibleProfileValues();
    if (!inaccessible_fields.empty()) {
      // We need to create a copy, because otherwise the internally stored
      // profile in |personal_data_manager_| is modified, which should only
      // happen via UpdateProfile().
      AutofillProfile updated_profile = *profile;
      updated_profile.ClearFields(inaccessible_fields);
      personal_data_manager_->UpdateProfile(updated_profile);
    }
  }
}

bool PersonalDataManagerCleaner::ApplyDedupingRoutine() {
  // Check if de-duplication has already been performed on this major version.
  if (!is_autofill_profile_dedupe_pending_) {
    return false;
  }

  const std::vector<AutofillProfile*>& profiles =
      base::FeatureList::IsEnabled(
                  features::kAutofillAccountProfileStorage)
          ? personal_data_manager_->GetProfiles()
          : personal_data_manager_->GetProfilesFromSource(
                AutofillProfile::Source::kLocalOrSyncable);

  // No need to de-duplicate if there are less than two profiles.
  if (profiles.size() < 2) {
    DVLOG(1) << "Autofill profile de-duplication not needed.";
    pref_service_->SetInteger(prefs::kAutofillLastVersionDeduped,
                              CHROME_VERSION_MAJOR);
    return false;
  }

  DVLOG(1) << "Starting autofill profile de-duplication.";
  std::unordered_set<std::string> profiles_to_delete;
  profiles_to_delete.reserve(profiles.size());

  // `profiles` contains pointers to the PDM's state. Modifying them directly
  // won't update them in the database and calling `PDM:UpdateProfile()`
  // would discard them as a duplicate.
  std::vector<std::unique_ptr<AutofillProfile>> new_profiles;
  for (AutofillProfile* profile : profiles) {
    new_profiles.push_back(std::make_unique<AutofillProfile>(*profile));
  }

  DedupeProfiles(&new_profiles, &profiles_to_delete);

  // Apply the profile changes to the database.
  for (const auto& profile : new_profiles) {
    // If the profile was set to be deleted, remove it from the database,
    // otherwise update it.
    if (profiles_to_delete.contains(profile->guid())) {
      personal_data_manager_->RemoveProfileFromDB(profile->guid());
    } else {
      personal_data_manager_->UpdateProfileInDB(*profile);
    }
  }

  is_autofill_profile_dedupe_pending_ = false;
  // Set the pref to the current major version.
  pref_service_->SetInteger(prefs::kAutofillLastVersionDeduped,
                            CHROME_VERSION_MAJOR);

  return true;
}

void PersonalDataManagerCleaner::DedupeProfiles(
    std::vector<std::unique_ptr<AutofillProfile>>* existing_profiles,
    std::unordered_set<std::string>* profiles_to_delete) const {
  AutofillMetrics::LogNumberOfProfilesConsideredForDedupe(
      existing_profiles->size());

  // Sort the profiles by ranking score. That way the most relevant profiles
  // will get merged into the less relevant profiles, which keeps the syntax of
  // the most relevant profiles data.
  // Since profiles earlier in the list are merged into profiles later in the
  // list, `kLocalOrSyncable` profiles are placed before `kAccount` profiles.
  // This is because local profiles can be merged into account profiles, but not
  // the other way around.
  // TODO(crbug.com/1411114): Remove code duplication for sorting profiles.
  base::ranges::sort(
      *existing_profiles, [comparison_time = AutofillClock::Now()](
                              const std::unique_ptr<AutofillProfile>& a,
                              const std::unique_ptr<AutofillProfile>& b) {
        if (a->source() != b->source()) {
          return a->source() == AutofillProfile::Source::kLocalOrSyncable;
        }
        return a->HasGreaterRankingThan(b.get(), comparison_time);
      });

  AutofillProfileComparator comparator(personal_data_manager_->app_locale());
  for (auto i = existing_profiles->begin(); i != existing_profiles->end();
       i++) {
    AutofillProfile* profile_to_merge = i->get();

    // If the profile was set to be deleted, skip it. This can happen because
    // the loop below reassigns `profile_to_merge` to (effectively) `j->get()`.
    if (profiles_to_delete->contains(profile_to_merge->guid())) {
      continue;
    }

    // Profiles in the account storage should not be silently deleted.
    if (profile_to_merge->source() == AutofillProfile::Source::kAccount) {
      continue;
    }

    // Try to merge `profile_to_merge` with a less relevant `existing_profiles`.
    for (auto j = i + 1; j < existing_profiles->end(); j++) {
      AutofillProfile& existing_profile = **j;

      // Don't try to merge a profile that was already set for deletion or that
      // cannot be merged.
      if (profiles_to_delete->contains(existing_profile.guid()) ||
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

      // The profiles are found to be mergeable. Attempt to update the existing
      // profile. This returns true if the merge was successful, or if the
      // merge would have been successful but the existing profile IsVerified()
      // and will not accept updates from `profile_to_merge`.
      if (existing_profile.SaveAdditionalInfo(
              *profile_to_merge, personal_data_manager_->app_locale())) {
        profiles_to_delete->insert(profile_to_merge->guid());

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
  }
  AutofillMetrics::LogNumberOfProfilesRemovedDuringDedupe(
      profiles_to_delete->size());
}

bool PersonalDataManagerCleaner::DeleteDisusedAddresses() {
  const std::vector<AutofillProfile*>& profiles =
      personal_data_manager_->GetProfilesFromSource(
          AutofillProfile::Source::kLocalOrSyncable);

  // Early exit when there are no profiles.
  if (profiles.empty()) {
    DVLOG(1) << "There are no profiles";
    return true;
  }

  // Don't call `PDM::RemoveByGUID()` directly, since this can invalidate the
  // pointers in `profiles`.
  std::vector<std::string> guids_to_delete;
  for (AutofillProfile* profile : profiles) {
    if (profile->IsDeletable()) {
      guids_to_delete.push_back(profile->guid());
    }
  }

  size_t num_deleted_addresses = guids_to_delete.size();

  for (auto const& guid : guids_to_delete) {
    personal_data_manager_->RemoveByGUID(guid);
  }

  AutofillMetrics::LogNumberOfAddressesDeletedForDisuse(num_deleted_addresses);

  return true;
}

void PersonalDataManagerCleaner::ClearCreditCardNonSettingsOrigins() {
  bool has_updated = false;

  for (CreditCard* card : personal_data_manager_->GetLocalCreditCards()) {
    if (card->origin() != kSettingsOrigin && !card->origin().empty()) {
      card->set_origin(std::string());
      personal_data_manager_->GetLocalDatabase()->UpdateCreditCard(*card);
      has_updated = true;
    }
  }

  // Refresh the local cache and send notifications to observers if a changed
  // was made.
  if (has_updated)
    personal_data_manager_->Refresh();
}

bool PersonalDataManagerCleaner::DeleteDisusedCreditCards() {
  // Only delete local cards, as server cards are managed by Payments.
  auto cards = personal_data_manager_->GetLocalCreditCards();

  // Early exit when there is no local cards.
  if (cards.empty())
    return true;

  std::vector<CreditCard> cards_to_delete;
  for (CreditCard* card : cards) {
    if (card->IsDeletable()) {
      cards_to_delete.push_back(*card);
    }
  }

  size_t num_deleted_cards = cards_to_delete.size();

  if (num_deleted_cards > 0)
    personal_data_manager_->DeleteLocalCreditCards(cards_to_delete);

  AutofillMetrics::LogNumberOfCreditCardsDeletedForDisuse(num_deleted_cards);

  return true;
}

}  // namespace autofill
