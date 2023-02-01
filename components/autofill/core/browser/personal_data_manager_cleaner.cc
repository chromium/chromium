// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager_cleaner.h"

#include "base/logging.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/features.h"

namespace autofill {

PersonalDataManagerCleaner::PersonalDataManagerCleaner(
    PersonalDataManager* personal_data_manager,
    AlternativeStateNameMapUpdater* alternative_state_name_map_updater,
    PrefService* pref_service)
    : test_data_creator_(kDisusedDataModelDeletionTimeDelta,
                         personal_data_manager->app_locale()),
      personal_data_manager_(personal_data_manager),
      pref_service_(pref_service),
      alternative_state_name_map_updater_(alternative_state_name_map_updater) {
  // Check if profile cleanup has already been performed this major version.
  is_autofill_profile_cleanup_pending_ =
      pref_service_->GetInteger(prefs::kAutofillLastVersionDeduped) <
      CHROME_VERSION_MAJOR;
  DVLOG(1) << "Autofill profile cleanup "
           << (is_autofill_profile_cleanup_pending_ ? "needs to be"
                                                    : "has already been")
           << " performed for this version";
}

PersonalDataManagerCleaner::~PersonalDataManagerCleaner() = default;

void PersonalDataManagerCleaner::CleanupDataAndNotifyPersonalDataObservers() {
  // The profile de-duplication is run once every major chrome version. If the
  // profile de-duplication has not run for the |CHROME_VERSION_MAJOR| yet,
  // |AlternativeStateNameMap| needs to be populated first. Otherwise,
  // defer the insertion to when the observers are notified.
  if (!alternative_state_name_map_updater_
           ->is_alternative_state_name_map_populated() &&
      base::FeatureList::IsEnabled(
          features::kAutofillUseAlternativeStateNameMap) &&
      is_autofill_profile_cleanup_pending_) {
    alternative_state_name_map_updater_->PopulateAlternativeStateNameMap(
        base::BindOnce(&PersonalDataManagerCleaner::
                           CleanupDataAndNotifyPersonalDataObservers,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // If sync is enabled for addresses, defer running cleanups until address
  // sync has started; otherwise, do it now.
  if (!personal_data_manager_->IsSyncEnabledFor(syncer::AUTOFILL_PROFILE))
    ApplyAddressFixesAndCleanups();

  // If sync is enabled for credit cards, defer running cleanups until card
  // sync has started; otherwise, do it now.
  if (!personal_data_manager_->IsSyncEnabledFor(syncer::AUTOFILL_WALLET_DATA))
    ApplyCardFixesAndCleanups();

  // Log address, credit card and offer startup metrics.
  personal_data_manager_->LogStoredProfileMetrics();
  personal_data_manager_->LogStoredCreditCardMetrics();
  personal_data_manager_->LogStoredOfferMetrics();

  personal_data_manager_->NotifyPersonalDataObserver();
}

void PersonalDataManagerCleaner::SyncStarted(syncer::ModelType model_type) {
  // The profile de-duplication is run once every major chrome version. If the
  // profile de-duplication has not run for the |CHROME_VERSION_MAJOR| yet,
  // |AlternativeStateNameMap| needs to be populated first. Otherwise,
  // defer the insertion to when the observers are notified.
  // TODO(crbug.com/1111960): If sync is disabled and re-enabled, the
  // alternative state name map should be re-populated. This is currently not
  // the case due to the `is_alternative_state_name_map_populated()` check. This
  // state should be reset when sync is disabled, together with
  // `autofill_profile_sync_started` and `contact_info_sync_started`.
  autofill_profile_sync_started |= model_type == syncer::AUTOFILL_PROFILE;
  contact_info_sync_started |= model_type == syncer::CONTACT_INFO;
  if (autofill_profile_sync_started &&
      (contact_info_sync_started ||
       !base::FeatureList::IsEnabled(syncer::kSyncEnableContactInfoDataType)) &&
      !alternative_state_name_map_updater_
           ->is_alternative_state_name_map_populated() &&
      base::FeatureList::IsEnabled(
          features::kAutofillUseAlternativeStateNameMap) &&
      is_autofill_profile_cleanup_pending_) {
    alternative_state_name_map_updater_->PopulateAlternativeStateNameMap(
        base::BindOnce(&PersonalDataManagerCleaner::SyncStarted,
                       weak_ptr_factory_.GetWeakPtr(), model_type));
    return;
  }

  // Run deferred autofill address profile startup code.
  // See: PersonalDataManager::OnSyncServiceInitialized
  if (model_type == syncer::AUTOFILL_PROFILE)
    ApplyAddressFixesAndCleanups();

  // Run deferred credit card startup code.
  // See: PersonalDataManager::OnSyncServiceInitialized
  if (model_type == syncer::AUTOFILL_WALLET_DATA)
    ApplyCardFixesAndCleanups();
}

void PersonalDataManagerCleaner::ApplyAddressFixesAndCleanups() {
  // One-time fix, otherwise NOP.
  RemoveOrphanAutofillTableRows();

  // Once per major version, otherwise NOP.
  ApplyDedupingRoutine();

  // Once per major version, otherwise NOP.
  DeleteDisusedAddresses();

  // If feature AutofillCreateDataForTest is enabled, and once per user profile
  // startup.
  test_data_creator_.MaybeAddTestProfiles(base::BindRepeating(
      &PersonalDataManagerCleaner::AddProfileForTest, base::Unretained(this)));

  // Ran everytime it is called.
  ClearProfileNonSettingsOrigins();

  // Once per user profile startup.
  RemoveInaccessibleProfileValues();
}

void PersonalDataManagerCleaner::ApplyCardFixesAndCleanups() {
  // Once per major version, otherwise NOP.
  DeleteDisusedCreditCards();

  // If feature AutofillCreateDataForTest is enabled, and once per user profile
  // startup.
  test_data_creator_.MaybeAddTestCreditCards(
      base::BindRepeating(&PersonalDataManagerCleaner::AddCreditCardForTest,
                          base::Unretained(this)));

  // Ran everytime it is called.
  ClearCreditCardNonSettingsOrigins();
}

void PersonalDataManagerCleaner::RemoveOrphanAutofillTableRows() {
  // Don't run if the fix has already been applied.
  if (pref_service_->GetBoolean(prefs::kAutofillOrphanRowsRemoved))
    return;

  scoped_refptr<AutofillWebDataService> local_db =
      personal_data_manager_->GetLocalDatabase();
  if (!local_db)
    return;

  local_db->RemoveOrphanAutofillTableRows();

  // Set the pref so that this fix is never run again.
  pref_service_->SetBoolean(prefs::kAutofillOrphanRowsRemoved, true);
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
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableProfileDeduplication)) {
    return false;
  }

  // Check if de-duplication has already been performed on this major version.
  if (!is_autofill_profile_cleanup_pending_) {
    DVLOG(1)
        << "Autofill profile de-duplication already performed for this version";
    return false;
  }

  const std::vector<AutofillProfile*>& profiles =
      personal_data_manager_->GetProfilesFromSource(
          AutofillProfile::Source::kLocalOrSyncable);

  // No need to de-duplicate if there are less than two profiles.
  if (profiles.size() < 2) {
    DVLOG(1) << "Autofill profile de-duplication not needed.";
    return false;
  }

  DVLOG(1) << "Starting autofill profile de-duplication.";
  std::unordered_set<std::string> profiles_to_delete;
  profiles_to_delete.reserve(profiles.size());

  // Create the map used to update credit card's billing addresses after the
  // dedupe.
  std::unordered_map<std::string, std::string> guids_merge_map;

  // The changes can't happen directly on the profiles, but need to be
  // updated in the database at first, and then updated on the profiles.
  // Therefore, we need a copy of profiles to keep track of the changes.
  std::vector<std::unique_ptr<AutofillProfile>> new_profiles;
  for (auto* it : profiles) {
    new_profiles.push_back(std::make_unique<AutofillProfile>(*it));
  }

  DedupeProfiles(&new_profiles, &profiles_to_delete, &guids_merge_map);

  // Apply the profile changes to the database.
  for (const auto& profile : new_profiles) {
    // If the profile was set to be deleted, remove it from the database,
    // otherwise update it.
    if (profiles_to_delete.count(profile->guid())) {
      personal_data_manager_->RemoveProfileFromDB(profile->guid());
    } else {
      personal_data_manager_->UpdateProfileInDB(*(profile.get()));
    }
  }

  UpdateCardsBillingAddressReference(guids_merge_map);

  is_autofill_profile_cleanup_pending_ = false;
  // Set the pref to the current major version.
  pref_service_->SetInteger(prefs::kAutofillLastVersionDeduped,
                            CHROME_VERSION_MAJOR);

  return true;
}

void PersonalDataManagerCleaner::DedupeProfiles(
    std::vector<std::unique_ptr<AutofillProfile>>* existing_profiles,
    std::unordered_set<std::string>* profiles_to_delete,
    std::unordered_map<std::string, std::string>* guids_merge_map) const {
  AutofillMetrics::LogNumberOfProfilesConsideredForDedupe(
      existing_profiles->size());

  // Sort the profiles by ranking score with all the verified profiles at the
  // end. That way the most relevant profiles will get merged into the less
  // relevant profiles, which keeps the syntax of the most relevant profiles
  // data. Verified profiles are put at the end because they do not merge into
  // other profiles, so the loop can be stopped when we reach those. However
  // they need to be in the vector because an unverified profile trying to merge
  // into a similar verified profile will be discarded.
  // TODO(crbug.com/1411114): Remove code duplication for sorting profiles.
  const base::Time comparison_time = AutofillClock::Now();
  if (existing_profiles->size() > 1) {
    std::sort(existing_profiles->begin(), existing_profiles->end(),
              [comparison_time](const std::unique_ptr<AutofillProfile>& a,
                                const std::unique_ptr<AutofillProfile>& b) {
                if (a->IsVerified() != b->IsVerified()) {
                  return !a->IsVerified();
                }
                return a->HasGreaterRankingThan(b.get(), comparison_time);
              });
  }

  AutofillProfileComparator comparator(personal_data_manager_->app_locale());

  for (size_t i = 0; i < existing_profiles->size(); ++i) {
    AutofillProfile* profile_to_merge = (*existing_profiles)[i].get();

    // If the profile was set to be deleted, skip it. It has already been
    // merged into another profile.
    if (profiles_to_delete->count(profile_to_merge->guid()))
      continue;

    // If we have reached the verified profiles, stop trying to merge. Verified
    // profiles do not get merged.
    if (profile_to_merge->IsVerified())
      break;

    // If we have not reached the last profile, try to merge |profile_to_merge|
    // with all the less relevant |existing_profiles|.
    for (size_t j = i + 1; j < existing_profiles->size(); ++j) {
      AutofillProfile* existing_profile = (*existing_profiles)[j].get();

      // Don't try to merge a profile that was already set for deletion.
      if (profiles_to_delete->count(existing_profile->guid()))
        continue;

      // Move on if the profiles are not mergeable.
      if (!comparator.AreMergeable(*existing_profile, *profile_to_merge))
        continue;

      // The profiles are found to be mergeable. Attempt to update the existing
      // profile. This returns true if the merge was successful, or if the
      // merge would have been successful but the existing profile IsVerified()
      // and will not accept updates from profile_to_merge.
      if (existing_profile->SaveAdditionalInfo(
              *profile_to_merge, personal_data_manager_->app_locale())) {
        // Keep track that a credit card using |profile_to_merge|'s GUID as its
        // billing address id should replace it by |existing_profile|'s GUID.
        guids_merge_map->emplace(profile_to_merge->guid(),
                                 existing_profile->guid());

        // Since |profile_to_merge| was a duplicate of |existing_profile|
        // and was merged successfully, it can now be deleted.
        profiles_to_delete->insert(profile_to_merge->guid());

        // Now try to merge the new resulting profile with the rest of the
        // existing profiles.
        profile_to_merge = existing_profile;

        // Verified profiles do not get merged. Save some time by not
        // trying.
        if (profile_to_merge->IsVerified())
          break;
      }
    }
  }
  AutofillMetrics::LogNumberOfProfilesRemovedDuringDedupe(
      profiles_to_delete->size());
}

void PersonalDataManagerCleaner::UpdateCardsBillingAddressReference(
    const std::unordered_map<std::string, std::string>& guids_merge_map) {
  /*  Here is an example of what the graph might look like.

      A -> B
             \
               -> E
             /
      C -> D
  */

  std::vector<CreditCard> server_cards_to_be_updated;
  for (auto* credit_card : personal_data_manager_->GetCreditCards()) {
    // If the credit card is not associated with a billing address, skip it.
    if (credit_card->billing_address_id().empty())
      continue;

    // If the billing address profile associated with the card has been merged,
    // replace it by the id of the profile in which it was merged. Repeat the
    // process until the billing address has not been merged into another one.
    std::unordered_map<std::string, std::string>::size_type nb_guid_changes = 0;
    bool was_modified = false;
    auto it = guids_merge_map.find(credit_card->billing_address_id());
    while (it != guids_merge_map.end()) {
      was_modified = true;
      credit_card->set_billing_address_id(it->second);
      it = guids_merge_map.find(credit_card->billing_address_id());

      // Out of abundance of caution.
      if (nb_guid_changes > guids_merge_map.size()) {
        NOTREACHED();
        // Cancel the changes for that card.
        was_modified = false;
        break;
      }
    }

    // If the card was modified, apply the changes to the database.
    if (was_modified) {
      if (credit_card->record_type() == CreditCard::LOCAL_CARD)
        personal_data_manager_->GetLocalDatabase()->UpdateCreditCard(
            *credit_card);
      else
        server_cards_to_be_updated.push_back(*credit_card);
    }
  }

  // In case, there are server cards that need to be updated,
  // |PersonalDataManager::Refresh()| is called after they are updated.
  if (server_cards_to_be_updated.empty())
    personal_data_manager_->Refresh();
  else
    personal_data_manager_->UpdateServerCardsMetadata(
        server_cards_to_be_updated);
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

  std::unordered_set<std::string> used_billing_address_guids;
  for (CreditCard* card : personal_data_manager_->GetCreditCards()) {
    if (!card->IsDeletable()) {
      used_billing_address_guids.insert(card->billing_address_id());
    }
  }

  std::vector<std::string> guids_to_delete;
  for (AutofillProfile* profile : profiles) {
    if (profile->IsDeletable() &&
        !used_billing_address_guids.count(profile->guid())) {
      guids_to_delete.push_back(profile->guid());
    }
  }

  size_t num_deleted_addresses = guids_to_delete.size();

  for (auto const& guid : guids_to_delete) {
    personal_data_manager_
        ->RemoveAutofillProfileByGUIDAndBlankCreditCardReference(guid);
  }

  if (num_deleted_addresses > 0) {
    personal_data_manager_->Refresh();
  }

  AutofillMetrics::LogNumberOfAddressesDeletedForDisuse(num_deleted_addresses);

  return true;
}

void PersonalDataManagerCleaner::ClearProfileNonSettingsOrigins() {
  // `kAccount` profiles don't store an origin.
  for (AutofillProfile* profile : personal_data_manager_->GetProfilesFromSource(
           AutofillProfile::Source::kLocalOrSyncable)) {
    if (profile->origin() != kSettingsOrigin && !profile->origin().empty()) {
      profile->set_origin(std::string());
      personal_data_manager_->UpdateProfileInDB(*profile, /*enforced=*/true);
    }
  }
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

void PersonalDataManagerCleaner::AddProfileForTest(
    const AutofillProfile& profile) {
  personal_data_manager_->AddProfile(profile);
}

void PersonalDataManagerCleaner::AddCreditCardForTest(
    const CreditCard& credit_card) {
  personal_data_manager_->AddCreditCard(credit_card);
}

}  // namespace autofill
