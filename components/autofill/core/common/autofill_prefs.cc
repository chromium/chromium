// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace autofill {
namespace prefs {

// Integer that is set to the last choice user made when prompted for saving a
// credit card. The prompt is for user's consent in saving the card in the
// server for signed in users and saving the card locally for non signed-in
// users.
const char kAutofillAcceptSaveCreditCardPromptState[] =
    "autofill.accept_save_credit_card_prompt_state";

// Integer that is set to the billing customer number fetched from priority
// preference.
const char kAutofillBillingCustomerNumber[] = "billing_customer_number";

// The field type, validity state map of all profiles.
const char kAutofillProfileValidity[] = "autofill.profile_validity";

// Boolean that is true if Autofill is enabled and allowed to save credit card
// data.
const char kAutofillCreditCardEnabled[] = "autofill.credit_card_enabled";

// Number of times the credit card signin promo has been shown.
const char kAutofillCreditCardSigninPromoImpressionCount[] =
    "autofill.credit_card_signin_promo_impression_count";

// Boolean that is true if Autofill is enabled and allowed to save data.
const char kAutofillEnabledDeprecated[] = "autofill.enabled";

// Boolean that is true if Japan address city field has been migrated to be a
// part of the street field.
const char kAutofillJapanCityFieldMigrated[] =
    "autofill.japan_city_field_migrated_to_street_address";

// Integer that is set to the last version where the profile deduping routine
// was run. This routine will be run once per version.
const char kAutofillLastVersionDeduped[] = "autofill.last_version_deduped";

// Integer that is set to the last version where disused addresses were
// deleted. This deletion will be run once per version.
const char kAutofillLastVersionDisusedAddressesDeleted[] =
    "autofill.last_version_disused_addresses_deleted";

// Integer that is set to the last version where disused credit cards were
// deleted. This deletion will be run once per version.
const char kAutofillLastVersionDisusedCreditCardsDeleted[] =
    "autofill.last_version_disused_credit_cards_deleted";

// Boolean that is set to denote whether user cancelled/rejected local card
// migration prompt.
const char kAutofillMigrateLocalCardsCancelledPrompt[] =
    "autofill.migrate_local_card_cancelled_state";

// Boolean that is true if the orphan rows in the autofill table were removed.
const char kAutofillOrphanRowsRemoved[] = "autofill.orphan_rows_removed";

// Boolean that is true if Autofill is enabled and allowed to save profile data.
const char kAutofillProfileEnabled[] = "autofill.profile_enabled";

// Dictionary pref used to track which form signature uploads have been
// performed. Each entry in the dictionary maps a form signature (reduced
// via a 10-bit modulus) to a integer bit-field where each bit denotes whether
// or not a given upload event has occurred.
const char kAutofillUploadEvents[] = "autofill.upload_events";

// The timestamp (seconds since the Epoch UTC) for when the the upload event
// pref was last reset.
const char kAutofillUploadEventsLastResetTimestamp[] =
    "autofill.upload_events_last_reset_timestamp";

// Boolean that's true when Wallet card and address import is enabled by the
// user.
const char kAutofillWalletImportEnabled[] = "autofill.wallet_import_enabled";

// Boolean that is set to the last choice user made when prompted for saving an
// unmasked server card locally.
const char kAutofillWalletImportStorageCheckboxState[] =
    "autofill.wallet_import_storage_checkbox_state";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // Synced prefs. Used for cross-device choices, e.g., credit card Autofill.
  registry->RegisterDoublePref(
      prefs::kAutofillBillingCustomerNumber, 0.0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kAutofillEnabledDeprecated, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAutofillProfileEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kAutofillLastVersionDeduped, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kAutofillLastVersionDisusedAddressesDeleted, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAutofillCreditCardEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kAutofillProfileValidity, "",
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);

  // Non-synced prefs. Used for per-device choices, e.g., signin promo.
  registry->RegisterIntegerPref(
      prefs::kAutofillCreditCardSigninPromoImpressionCount, 0);
  registry->RegisterBooleanPref(prefs::kAutofillJapanCityFieldMigrated, false);
  registry->RegisterBooleanPref(prefs::kAutofillWalletImportEnabled, true);
  registry->RegisterBooleanPref(
      prefs::kAutofillWalletImportStorageCheckboxState, true);
  registry->RegisterIntegerPref(
      prefs::kAutofillAcceptSaveCreditCardPromptState,
      prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_NONE);
  registry->RegisterIntegerPref(
      prefs::kAutofillLastVersionDisusedCreditCardsDeleted, 0);
  registry->RegisterBooleanPref(
      prefs::kAutofillMigrateLocalCardsCancelledPrompt, false);
  registry->RegisterBooleanPref(prefs::kAutofillOrphanRowsRemoved, false);
  registry->RegisterDictionaryPref(prefs::kAutofillUploadEvents);
  registry->RegisterTimePref(prefs::kAutofillUploadEventsLastResetTimestamp,
                             base::Time());
}

void MigrateDeprecatedAutofillPrefs(PrefService* prefs) {
  // If kAutofillCreditCardEnabled and kAutofillProfileEnabled prefs are
  // currently using their default value and kAutofillEnabledDeprecated has a
  // non-default value, override the valuAues of the new prefs. The following
  // blocks should execute only once and are needed for those users who had
  // Autofill disabled before introduction of the fine-grained prefs.
  // TODO(crbug.com/870328): Remove these once M70- users are sufficiently low.
  const PrefService::Preference* deprecated_autofill_pref =
      prefs->FindPreference(prefs::kAutofillEnabledDeprecated);
  DCHECK(deprecated_autofill_pref);

  const PrefService::Preference* autofill_credit_card_pref =
      prefs->FindPreference(prefs::kAutofillCreditCardEnabled);
  DCHECK(autofill_credit_card_pref);
  if (autofill_credit_card_pref->IsDefaultValue() &&
      !deprecated_autofill_pref->IsDefaultValue()) {
    prefs->SetBoolean(kAutofillCreditCardEnabled,
                      prefs->GetBoolean(kAutofillEnabledDeprecated));
  }

  const PrefService::Preference* autofill_profile_pref =
      prefs->FindPreference(prefs::kAutofillProfileEnabled);
  DCHECK(autofill_profile_pref);
  if (autofill_profile_pref->IsDefaultValue() &&
      !deprecated_autofill_pref->IsDefaultValue()) {
    prefs->SetBoolean(kAutofillProfileEnabled,
                      prefs->GetBoolean(kAutofillEnabledDeprecated));
  }
}

bool IsAutocompleteEnabled(const PrefService* prefs) {
  return IsProfileAutofillEnabled(prefs);
}

bool IsAutofillEnabled(const PrefService* prefs) {
  return IsProfileAutofillEnabled(prefs) || IsCreditCardAutofillEnabled(prefs);
}

void SetAutofillEnabled(PrefService* prefs, bool enabled) {
  SetProfileAutofillEnabled(prefs, enabled);
  SetCreditCardAutofillEnabled(prefs, enabled);
}

bool IsCreditCardAutofillEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kAutofillCreditCardEnabled);
}

void SetCreditCardAutofillEnabled(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kAutofillCreditCardEnabled, enabled);
}

bool IsAutofillManaged(const PrefService* prefs) {
  return prefs->IsManagedPreference(kAutofillEnabledDeprecated);
}

bool IsProfileAutofillManaged(const PrefService* prefs) {
  return prefs->IsManagedPreference(kAutofillProfileEnabled);
}

bool IsCreditCardAutofillManaged(const PrefService* prefs) {
  return prefs->IsManagedPreference(kAutofillCreditCardEnabled);
}

bool IsProfileAutofillEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kAutofillProfileEnabled);
}

void SetProfileAutofillEnabled(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kAutofillProfileEnabled, enabled);
}

bool IsLocalCardMigrationPromptPreviouslyCancelled(const PrefService* prefs) {
  return prefs->GetBoolean(kAutofillMigrateLocalCardsCancelledPrompt);
}

void SetLocalCardMigrationPromptPreviouslyCancelled(PrefService* prefs,
                                                    bool enabled) {
  prefs->SetBoolean(kAutofillMigrateLocalCardsCancelledPrompt, enabled);
}

bool IsPaymentsIntegrationEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kAutofillWalletImportEnabled);
}

void SetPaymentsIntegrationEnabled(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kAutofillWalletImportEnabled, enabled);
}

std::string GetAllProfilesValidityMapsEncodedString(const PrefService* prefs) {
  return prefs->GetString(kAutofillProfileValidity);
}

}  // namespace prefs
}  // namespace autofill
