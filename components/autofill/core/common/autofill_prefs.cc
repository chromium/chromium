// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_prefs.h"

#include "base/base64.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "crypto/sha2.h"

namespace autofill {
namespace prefs {
namespace {

// Returns the opt-in bitfield for the specific |account_id| or 0 if no entry
// was found.
int GetSyncTransportOptInBitFieldForAccount(const PrefService* prefs,
                                            const std::string& account_hash) {
  auto* dictionary = prefs->GetDictionary(prefs::kAutofillSyncTransportOptIn);

  // If there is no dictionary it means the account didn't opt-in. Use 0 because
  // it's the same as not having opted-in to anything.
  if (!dictionary) {
    return 0;
  }

  // If there is no entry in the dictionary, it means the account didn't opt-in.
  // Use 0 because it's the same as not having opted-in to anything.
  auto* found =
      dictionary->FindKeyOfType(account_hash, base::Value::Type::INTEGER);
  return found ? found->GetInt() : 0;
}

}  // namespace

// Integer that is set to the last choice user made when prompted for saving a
// credit card. The prompt is for user's consent in saving the card in the
// server for signed in users and saving the card locally for non signed-in
// users.
const char kAutofillAcceptSaveCreditCardPromptState[] =
    "autofill.accept_save_credit_card_prompt_state";

// Boolean that is true if Autofill is enabled and allowed to save credit card
// data.
const char kAutofillCreditCardEnabled[] = "autofill.credit_card_enabled";

// Boolean that is true if FIDO Authentication is enabled for card unmasking.
const char kAutofillCreditCardFidoAuthEnabled[] =
    "autofill.credit_card_fido_auth_enabled";

#if defined(OS_ANDROID)
// Boolean that is true if FIDO Authentication is enabled for card unmasking.
const char kAutofillCreditCardFidoAuthOfferCheckboxState[] =
    "autofill.credit_card_fido_auth_offer_checkbox_state";
#endif

// Number of times the credit card signin promo has been shown.
const char kAutofillCreditCardSigninPromoImpressionCount[] =
    "autofill.credit_card_signin_promo_impression_count";

// Boolean that is true if Autofill is enabled and allowed to save data.
const char kAutofillEnabledDeprecated[] = "autofill.enabled";

// Deprecated 10/2019.
const char kAutofillJapanCityFieldMigratedDeprecated[] =
    "autofill.japan_city_field_migrated_to_street_address";

// Integer that is set to the last version where the profile deduping routine
// was run. This routine will be run once per version.
const char kAutofillLastVersionDeduped[] = "autofill.last_version_deduped";

// Integer that is set to the last version where the profile validation routine
// was run. We validate profiles at least once per version to keep track of the
// changes in the validation logic.
const char kAutofillLastVersionValidated[] = "autofill.last_version_validated";

// Integer that is set to the last version where disused addresses were
// deleted. This deletion will be run once per version.
const char kAutofillLastVersionDisusedAddressesDeleted[] =
    "autofill.last_version_disused_addresses_deleted";

// Integer that is set to the last version where disused credit cards were
// deleted. This deletion will be run once per version.
const char kAutofillLastVersionDisusedCreditCardsDeleted[] =
    "autofill.last_version_disused_credit_cards_deleted";

// Boolean that is true if the orphan rows in the autofill table were removed.
const char kAutofillOrphanRowsRemoved[] = "autofill.orphan_rows_removed";

// Boolean that is true if Autofill is enabled and allowed to save profile data.
const char kAutofillProfileEnabled[] = "autofill.profile_enabled";

// The field type, validity state map of all profiles.
// TODO(crbug.com/910596): Pref name is "autofill_" instead of "autofill."
// because of a mismatch when the priorify prefs were generated. Consider
// migrating this back to "autofill." in the future.
const char kAutofillProfileValidity[] = "autofill_profile_validity";

// This pref stores the file path where the autofill states data is
// downloaded to.
const char kAutofillStatesDataDir[] = "autofill.states_data_dir";

// The opt-ins for Sync Transport features for each client.
const char kAutofillSyncTransportOptIn[] = "autofill.sync_transport_opt_ins";

// The (randomly inititialied) seed value to use when encoding form/field
// metadata for randomized uploads. The value of this pref is a string.
const char kAutofillUploadEncodingSeed[] = "autofill.upload_encoding_seed";

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

// Integer that is set to the last major version where the Autocomplete
// retention policy was run.
const char kAutocompleteLastVersionRetentionPolicy[] =
    "autocomplete.retention_policy_last_version";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // Synced prefs. Used for cross-device choices, e.g., credit card Autofill.
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
      prefs::kAutofillLastVersionValidated, 0,
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
  registry->RegisterBooleanPref(prefs::kAutofillCreditCardFidoAuthEnabled,
                                false);
#if defined(OS_ANDROID)
  registry->RegisterBooleanPref(
      prefs::kAutofillCreditCardFidoAuthOfferCheckboxState, true);
#endif
  registry->RegisterIntegerPref(
      prefs::kAutofillCreditCardSigninPromoImpressionCount, 0);
  registry->RegisterBooleanPref(prefs::kAutofillWalletImportEnabled, true);
  registry->RegisterBooleanPref(
      prefs::kAutofillWalletImportStorageCheckboxState, true);
  registry->RegisterIntegerPref(
      prefs::kAutofillAcceptSaveCreditCardPromptState,
      prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_NONE);
  registry->RegisterIntegerPref(
      prefs::kAutofillLastVersionDisusedCreditCardsDeleted, 0);
  registry->RegisterIntegerPref(prefs::kAutocompleteLastVersionRetentionPolicy,
                                0);
  registry->RegisterBooleanPref(prefs::kAutofillOrphanRowsRemoved, false);
  registry->RegisterStringPref(prefs::kAutofillUploadEncodingSeed, "");
  registry->RegisterDictionaryPref(prefs::kAutofillUploadEvents);
  registry->RegisterTimePref(prefs::kAutofillUploadEventsLastResetTimestamp,
                             base::Time());
  registry->RegisterDictionaryPref(prefs::kAutofillSyncTransportOptIn);
  registry->RegisterStringPref(prefs::kAutofillStatesDataDir, "");

  // Deprecated prefs registered for migration.
  registry->RegisterBooleanPref(kAutofillJapanCityFieldMigratedDeprecated,
                                false);
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

  // Added 10/2019.
  prefs->ClearPref(kAutofillJapanCityFieldMigratedDeprecated);
}

bool IsAutocompleteEnabled(const PrefService* prefs) {
  return IsAutofillProfileEnabled(prefs);
}

bool IsCreditCardFIDOAuthEnabled(PrefService* prefs) {
  return prefs->GetBoolean(kAutofillCreditCardFidoAuthEnabled);
}

void SetCreditCardFIDOAuthEnabled(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kAutofillCreditCardFidoAuthEnabled, enabled);
}

bool IsAutofillCreditCardEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kAutofillCreditCardEnabled);
}

void SetAutofillCreditCardEnabled(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kAutofillCreditCardEnabled, enabled);
}

bool IsAutofillManaged(const PrefService* prefs) {
  return prefs->IsManagedPreference(kAutofillEnabledDeprecated);
}

bool IsAutofillProfileManaged(const PrefService* prefs) {
  return prefs->IsManagedPreference(kAutofillProfileEnabled);
}

bool IsAutofillCreditCardManaged(const PrefService* prefs) {
  return prefs->IsManagedPreference(kAutofillCreditCardEnabled);
}

bool IsAutofillProfileEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kAutofillProfileEnabled);
}

void SetAutofillProfileEnabled(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kAutofillProfileEnabled, enabled);
}

bool IsPaymentsIntegrationEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kAutofillWalletImportEnabled);
}

void SetPaymentsIntegrationEnabled(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kAutofillWalletImportEnabled, enabled);
}

std::string GetAllProfilesValidityMapsEncodedString(const PrefService* prefs) {
  std::string value = prefs->GetString(kAutofillProfileValidity);
  if (base::Base64Decode(value, &value))
    return value;
  return std::string();
}

void SetUserOptedInWalletSyncTransport(PrefService* prefs,
                                       const CoreAccountId& account_id,
                                       bool opted_in) {
  // Get the hash of the account id. The hashing here is only a secondary bit of
  // obfuscation. The primary privacy guarantees are handled by clearing this
  // whenever cookies are cleared.
  std::string account_hash;
  base::Base64Encode(crypto::SHA256HashString(account_id.ToString()),
                     &account_hash);

  DictionaryPrefUpdate update(prefs, prefs::kAutofillSyncTransportOptIn);
  int value = GetSyncTransportOptInBitFieldForAccount(prefs, account_hash);

  // If the user has opted in, set that bit while leaving the others intact.
  if (opted_in) {
    update->SetKey(account_hash,
                   base::Value(value | sync_transport_opt_in::kWallet));
    return;
  }

  // Invert the mask in order to reset the Wallet bit while leaving the other
  // bits intact, or remove the key entirely if the Wallet was the only opt-in.
  if (value & ~sync_transport_opt_in::kWallet) {
    update->SetKey(account_hash,
                   base::Value(value & ~sync_transport_opt_in::kWallet));
  } else {
    update->RemoveKey(account_hash);
  }
}

bool IsUserOptedInWalletSyncTransport(const PrefService* prefs,
                                      const CoreAccountId& account_id) {
#if defined(OS_ANDROID) || defined(OS_IOS)
  // On mobile, no specific opt-in is required.
  return true;
#else
  // Get the hash of the account id.
  std::string account_hash;
  base::Base64Encode(crypto::SHA256HashString(account_id.ToString()),
                     &account_hash);

  // Return whether the wallet opt-in bit is set.
  return GetSyncTransportOptInBitFieldForAccount(prefs, account_hash) &
         sync_transport_opt_in::kWallet;
#endif  // OS_ANDROID || defined(OS_IOS)
}

void ClearSyncTransportOptIns(PrefService* prefs) {
  DictionaryPrefUpdate update(prefs, prefs::kAutofillSyncTransportOptIn);
  update->Clear();
}

}  // namespace prefs
}  // namespace autofill
