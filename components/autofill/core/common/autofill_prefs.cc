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
#include "components/prefs/pref_registry_simple.h"
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
  registry->RegisterBooleanPref(
      kAutofillPaymentCardBenefits, true,
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
  registry->RegisterDictionaryPref(prefs::kAutofillVoteUploadEvents);
  registry->RegisterDictionaryPref(prefs::kAutofillMetadataUploadEvents);
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
  registry->RegisterStringPref(kAutofillAblationSeedPref, "");
  registry->RegisterBooleanPref(
      prefs::kAutofillRanQuasiDuplicateExtraDeduplication, false);

#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(prefs::kAutofillUsingVirtualViewStructure,
                                false);
  registry->RegisterBooleanPref(
      prefs::kAutofillThirdPartyPasswordManagersAllowed, true);
  registry->RegisterBooleanPref(
      prefs::kFacilitatedPaymentsPix, /*default_value=*/true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  registry->RegisterBooleanPref(prefs::kAutofillPredictionImprovementsEnabled,
                                false);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
}

void MigrateDeprecatedAutofillPrefs(PrefService* pref_service) {
  // Added 09/2022.
  pref_service->ClearPref(prefs::kAutofillEnabledDeprecated);
  // Added 05/2023.
  pref_service->ClearPref(prefs::kAutofillOrphanRowsRemoved);
  // Added 09/2023.
  pref_service->ClearPref(prefs::kAutofillIbanEnabled);
  // Added 10/2023
  pref_service->ClearPref(prefs::kAutofillLastVersionDisusedAddressesDeleted);
  pref_service->ClearPref(prefs::kAutofillLastVersionDisusedCreditCardsDeleted);
  // Added 07/2024 (moved from profile pref to local state)
  pref_service->ClearPref(prefs::kAutofillAblationSeedPref);
  // Added 10/2024
  pref_service->ClearPref(prefs::kAutofillRanQuasiDuplicateExtraDeduplication);
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kAutofillAblationSeedPref, "");
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
  return prefs->GetBoolean(kAutofillPaymentMethodsMandatoryReauth);
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

bool IsPaymentCardBenefitsEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kAutofillPaymentCardBenefits);
}

void SetPaymentCardBenefits(PrefService* prefs, bool value) {
  prefs->SetBoolean(kAutofillPaymentCardBenefits, value);
}

void SetUserOptedInWalletSyncTransport(PrefService* prefs,
                                       const CoreAccountId& account_id,
                                       bool opted_in) {
  // Get the hash of the account id. The hashing here is only a secondary bit of
  // obfuscation. The primary privacy guarantees are handled by clearing this
  // whenever cookies are cleared.
  std::string account_hash =
      base::Base64Encode(crypto::SHA256HashString(account_id.ToString()));

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
  std::string account_hash =
      base::Base64Encode(crypto::SHA256HashString(account_id.ToString()));

  // Return whether the wallet opt-in bit is set.
  return GetSyncTransportOptInBitFieldForAccount(prefs, account_hash) &
         sync_transport_opt_in::kWallet;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

void ClearSyncTransportOptIns(PrefService* prefs) {
  prefs->SetDict(prefs::kAutofillSyncTransportOptIn, base::Value::Dict());
}

// UsesVirtualViewStructureForAutofill is defined in
// //chrome/browser/ui/autofill/autofill_client_provider.cc

void SetFacilitatedPaymentsPix(PrefService* prefs, bool value) {
#if BUILDFLAG(IS_ANDROID)
  prefs->SetBoolean(kFacilitatedPaymentsPix, value);
#endif  // BUILDFLAG(IS_ANDROID)
}

bool IsFacilitatedPaymentsPixEnabled(const PrefService* prefs) {
#if BUILDFLAG(IS_ANDROID)
  return prefs->GetBoolean(kFacilitatedPaymentsPix);
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace prefs
}  // namespace autofill
