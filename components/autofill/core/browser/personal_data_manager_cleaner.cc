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

  // If sync is enabled for autofill, defer running cleanups until address
  // sync and card sync have started; otherwise, do it now.
  if (!personal_data_manager_->IsSyncEnabledFor(
          syncer::UserSelectableType::kAutofill)) {
    ApplyAddressFixesAndCleanups();
    ApplyCardFixesAndCleanups();
  }

  // Log address, credit card, offer, IBAN, and usage data startup metrics.
  personal_data_manager_->LogStoredDataMetrics();

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
  if (model_type == syncer::AUTOFILL_PROFILE)
    ApplyAddressFixesAndCleanups();

  // Run deferred credit card startup code.
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

  // Ran everytime it is called.
  ClearProfileNonSettingsOrigins();

  // Once per user profile startup.
  RemoveInaccessibleProfileValues();
}

void PersonalDataManagerCleaner::ApplyCardFixesAndCleanups() {
  // Once per major version, otherwise NOP.
  DeleteDisusedCreditCards();

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
      base::FeatureList::IsEnabled(
          features::kAutofillAccountProfilesUnionView) &&
              base::FeatureList::IsEnabled(
                  features::kAutofillAccountProfileStorage)
          ? personal_data_manager_->GetProfiles()
          : personal_data_manager_->GetProfilesFromSource(
                AutofillProfile::Source::kLocalOrSyncable);

  // No need to de-duplicate if there are less than two profiles.
  if (profiles.size() < 2) {
    DVLOG(1) << "Autofill profile de-duplication not needed.";
    return false;
  }

  DVLOG(1) << "Starting autofill profile de-duplication.";
  std::unordered_set<std::string> profiles_to_delete;
  profiles_to_delete.reserve(profiles.size());
  // Used to update credit card's billing addresses after the dedupe.
  std::unordered_map<std::string, std::string> guids_merge_map;

  // `profiles` contains pointers to the PDM's state. Modifying them directly
  // won't update them in the database and calling `PDM:UpdateProfile()`
  // would discard them as a duplicate.
  std::vector<std::unique_ptr<AutofillProfile>> new_profiles;
  for (AutofillProfile* profile : profiles) {
    new_profiles.push_back(std::make_unique<AutofillProfile>(*profile));
  }

  DedupeProfiles(&new_profiles, &profiles_to_delete, &guids_merge_map);

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
  auto first_verified_profile = base::ranges::stable_partition(
      *existing_profiles, [](const std::unique_ptr<AutofillProfile>& profile) {
        return !profile->IsVerified();
      });

  AutofillProfileComparator comparator(personal_data_manager_->app_locale());
  for (auto i = existing_profiles->begin(); i != first_verified_profile; i++) {
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
        guids_merge_map->emplace(profile_to_merge->guid(),
                                 existing_profile.guid());
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
        // Verified profiles do not get merged. Save some time by not
        // trying. Note that the `existing_profile` (now `profile_to_merge`)
        // might be verified.
        // Similarly, account profiles cannot be merged into other profiles,
        // since that would delete them.
        if (profile_to_merge->IsVerified() ||
            profile_to_merge->source() == AutofillProfile::Source::kAccount) {
          break;
        }
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

}  // namespace autofill
