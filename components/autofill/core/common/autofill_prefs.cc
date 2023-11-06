// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_prefs.h"

#include "base/base64.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "crypto/sha2.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace autofill {
namespace prefs {
namespace {

// Returns the opt-in bitfield for the specific |account_id| or 0 if no entry
// was found.
int GetSyncTransportOptInBitFieldForAccount(const PrefService* prefs,
                                            const std::string& account_hash) {
  const auto& dictionary = prefs->GetDict(prefs::kAutofillSyncTransportOptIn);

  // If there is no entry in the dictionary, it means the account didn't opt-in.
  // Use 0 because it's the same as not having opted-in to anything.
  const auto found = dictionary.FindInt(account_hash);
  return found.value_or(0);
}

}  // namespace

// Boolean that is true if Autofill is enabled and allowed to save credit card
// data.
const char kAutofillCreditCardEnabled[] = "autofill.credit_card_enabled";

// Boolean that is true if FIDO Authentication is enabled for card unmasking.
const char kAutofillCreditCardFidoAuthEnabled[] =
    "autofill.credit_card_fido_auth_enabled";

#if BUILDFLAG(IS_ANDROID)
// Boolean that is true if FIDO Authentication is enabled for card unmasking.
const char kAutofillCreditCardFidoAuthOfferCheckboxState[] =
    "autofill.credit_card_fido_auth_offer_checkbox_state";
#endif

// Boolean that is true if Autofill is enabled and allowed to save data.
const char kAutofillEnabledDeprecated[] = "autofill.enabled";

// Boolean that is true if a form with an IBAN field has ever been submitted, or
// an IBAN has ever been saved via Chrome payments settings page. This helps to
// enable IBAN functionality for those users who are not in a country where IBAN
// is generally available but have used IBAN already.
const char kAutofillHasSeenIban[] = "autofill.has_seen_iban";

// Boolean that is true if Autofill is enabled and allowed to save IBAN data.
extern const char kAutofillIbanEnabled[] = "autofill.iban_enabled";

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

// Boolean that is true if the orphan rows in the autofill table were removed.
const char kAutofillOrphanRowsRemoved[] = "autofill.orphan_rows_removed";

// Boolean that is true, when users can save their CVCs.
const char kAutofillPaymentCvcStorage[] = "autofill.payment_cvc_storage";

// Boolean that is true if Autofill is enabled and allowed to save profile data.
const char kAutofillProfileEnabled[] = "autofill.profile_enabled";

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

// Integer that is set to the last major version where the Autocomplete
// retention policy was run.
const char kAutocompleteLastVersionRetentionPolicy[] =
    "autocomplete.retention_policy_last_version";

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_IOS)
// Boolean that is set when payment methods mandatory re-auth is enabled by the
// user.
const char kAutofillPaymentMethodsMandatoryReauth[] =
    "autofill.payment_methods_mandatory_reauth";
#endif  // #if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
        // || BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
// Integer that is incremented when the mandatory re-auth promo is shown. If
// this is less than `kMaxValueForMandatoryReauthPromoShownCounter`, that
// implies that the user has not yet decided whether or not to turn on the
// payments mandatory re-auth feature.
const char kAutofillPaymentMethodsMandatoryReauthPromoShownCounter[] =
    "autofill.payment_methods_mandatory_reauth_promo_counter";
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
// Boolean that is true iff Chrome only provdides a virtual view structure that
// Android Autofill providers can use for filling. This pref is profile bound
// since each profile may have a preference for filling. It is not syncable as
// the setup on each device requires steps outside the browser. Enabling this
// pref on a device without a proper provider may yield a surprising absence of
// filling.
const char kAutofillUsingVirtualViewStructure[] =
    "autofill.using_virtual_view_structure";
#endif  // BUILDFLAG(IS_ANDROID)

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // Synced prefs. Used for cross-device choices, e.g., credit card Autofill.
  registry->RegisterBooleanPref(
      prefs::kAutofillProfileEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kAutofillLastVersionDeduped, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAutofillHasSeenIban, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAutofillCreditCardEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAutofillPaymentCvcStorage, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Non-synced prefs. Used for per-device choices, e.g., signin promo.
  registry->RegisterBooleanPref(prefs::kAutofillCreditCardFidoAuthEnabled,
                                false);
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(
      prefs::kAutofillCreditCardFidoAuthOfferCheckboxState, true);
#endif
  registry->RegisterIntegerPref(prefs::kAutocompleteLastVersionRetentionPolicy,
                                0);
  registry->RegisterStringPref(prefs::kAutofillUploadEncodingSeed, "");
  registry->RegisterDictionaryPref(prefs::kAutofillUploadEvents);
  registry->RegisterTimePref(prefs::kAutofillUploadEventsLastResetTimestamp,
                             base::Time());
  registry->RegisterDictionaryPref(prefs::kAutofillSyncTransportOptIn);
#if BUILDFLAG(IS_ANDROID)
  // Automotive devices require stricter data protection for user privacy, so
  // mandatory reauth for autofill payment methods should always be enabled.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    registry->RegisterBooleanPref(prefs::kAutofillPaymentMethodsMandatoryReauth,
                                  true);
  } else {
    registry->RegisterBooleanPref(prefs::kAutofillPaymentMethodsMandatoryReauth,
                                  false);
  }
  registry->RegisterIntegerPref(
      prefs::kAutofillPaymentMethodsMandatoryReauthPromoShownCounter, 0);
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  registry->RegisterBooleanPref(prefs::kAutofillPaymentMethodsMandatoryReauth,
                                false);
  registry->RegisterIntegerPref(
      prefs::kAutofillPaymentMethodsMandatoryReauthPromoShownCounter, 0);
#elif BUILDFLAG(IS_IOS)
  registry->RegisterBooleanPref(prefs::kAutofillPaymentMethodsMandatoryReauth,
                                true);
#endif

  // Deprecated prefs registered for migration.
  registry->RegisterBooleanPref(prefs::kAutofillEnabledDeprecated, true);
  registry->RegisterBooleanPref(prefs::kAutofillOrphanRowsRemoved, false);
  registry->RegisterBooleanPref(prefs::kAutofillIbanEnabled, true);
  registry->RegisterIntegerPref(
      prefs::kAutofillLastVersionDisusedAddressesDeleted, 0);
  registry->RegisterIntegerPref(
      prefs::kAutofillLastVersionDisusedCreditCardsDeleted, 0);

#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(prefs::kAutofillUsingVirtualViewStructure,
                                false);
#endif
}

void MigrateDeprecatedAutofillPrefs(PrefService* pref_service) {
  // Added 09/2022.
  pref_service->ClearPref(prefs::kAutofillEnabledDeprecated);
  // Added 05/2023.
  pref_service->ClearPref(prefs::kAutofillOrphanRowsRemoved);
  // Added 09/2023.
  pref_service->ClearPref(prefs::kAutofillIbanEnabled);
  // Added 10/2024
  pref_service->ClearPref(prefs::kAutofillLastVersionDisusedAddressesDeleted);
  pref_service->ClearPref(prefs::kAutofillLastVersionDisusedCreditCardsDeleted);
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

bool IsAutofillPaymentMethodsEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kAutofillCreditCardEnabled);
}

void SetAutofillPaymentMethodsEnabled(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kAutofillCreditCardEnabled, enabled);
}

bool HasSeenIban(const PrefService* prefs) {
  return prefs->GetBoolean(kAutofillHasSeenIban);
}

// If called, always sets the pref to true, and once true, it will follow the
// user around forever.
void SetAutofillHasSeenIban(PrefService* prefs) {
  prefs->SetBoolean(kAutofillHasSeenIban, true);
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

bool IsPaymentMethodsMandatoryReauthEnabled(const PrefService* prefs) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_IOS)
  bool featureEnabled = base::FeatureList::IsEnabled(
      features::kAutofillEnablePaymentsMandatoryReauth);
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    featureEnabled = true;
  }
#endif  // BUILDFLAG(IS_ANDROID)
  return featureEnabled &&
         prefs->GetBoolean(kAutofillPaymentMethodsMandatoryReauth);
#else
  return false;
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_IOS)
}

void SetPaymentMethodsMandatoryReauthEnabled(PrefService* prefs, bool enabled) {
#if BUILDFLAG(IS_ANDROID)
  // The user should not be able to update the pref value on automotive devices.
  CHECK(!base::android::BuildInfo::GetInstance()->is_automotive());
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_IOS)
  prefs->SetBoolean(kAutofillPaymentMethodsMandatoryReauth, enabled);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_IOS)
}

bool IsPaymentMethodsMandatoryReauthSetExplicitly(const PrefService* prefs) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  return prefs->GetUserPrefValue(kAutofillPaymentMethodsMandatoryReauth) !=
         nullptr;
#else
  return false;
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
}

bool IsPaymentMethodsMandatoryReauthPromoShownCounterBelowMaxCap(
    const PrefService* prefs) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  return prefs->GetInteger(
             kAutofillPaymentMethodsMandatoryReauthPromoShownCounter) <
         kMaxValueForMandatoryReauthPromoShownCounter;
#else
  return false;
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
}

void IncrementPaymentMethodsMandatoryReauthPromoShownCounter(
    PrefService* prefs) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  if (prefs->GetInteger(
          kAutofillPaymentMethodsMandatoryReauthPromoShownCounter) >=
      kMaxValueForMandatoryReauthPromoShownCounter) {
    return;
  }

  prefs->SetInteger(
      kAutofillPaymentMethodsMandatoryReauthPromoShownCounter,
      prefs->GetInteger(
          kAutofillPaymentMethodsMandatoryReauthPromoShownCounter) +
          1);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
}

bool IsPaymentCvcStorageEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kAutofillPaymentCvcStorage);
}

void SetPaymentCvcStorage(PrefService* prefs, bool value) {
  prefs->SetBoolean(kAutofillPaymentCvcStorage, value);
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

  ScopedDictPrefUpdate update(prefs, prefs::kAutofillSyncTransportOptIn);
  int value = GetSyncTransportOptInBitFieldForAccount(prefs, account_hash);

  // If the user has opted in, set that bit while leaving the others intact.
  if (opted_in) {
    update->Set(account_hash, value | sync_transport_opt_in::kWallet);
    return;
  }

  // Invert the mask in order to reset the Wallet bit while leaving the other
  // bits intact, or remove the key entirely if the Wallet was the only opt-in.
  if (value & ~sync_transport_opt_in::kWallet) {
    update->Set(account_hash, value & ~sync_transport_opt_in::kWallet);
  } else {
    update->Remove(account_hash);
  }
}

bool IsUserOptedInWalletSyncTransport(const PrefService* prefs,
                                      const CoreAccountId& account_id) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
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
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

void ClearSyncTransportOptIns(PrefService* prefs) {
  prefs->SetDict(prefs::kAutofillSyncTransportOptIn, base::Value::Dict());
}

bool UsesVirtualViewStructureForAutofill(const PrefService* prefs) {
#if BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(
          features::kAutofillVirtualViewStructureAndroid)) {
    return false;
  }

  return prefs->GetBoolean(kAutofillUsingVirtualViewStructure);
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace prefs
}  // namespace autofill
