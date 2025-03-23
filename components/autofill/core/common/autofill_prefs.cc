// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_prefs.h"

#include "build/build_config.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace autofill::prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // Synced prefs. Used for cross-device choices, e.g., credit card Autofill.
  registry->RegisterBooleanPref(
      kAutofillProfileEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      kAutofillLastVersionDeduped, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kAutofillHasSeenIban, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kAutofillCreditCardEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kAutofillPaymentCvcStorage, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kAutofillPaymentCardBenefits, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Non-synced prefs. Used for per-device choices, e.g., signin promo.
  registry->RegisterDictionaryPref(kAutofillAiOptInStatus);
  registry->RegisterBooleanPref(kAutofillCreditCardFidoAuthEnabled, false);
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(kAutofillCreditCardFidoAuthOfferCheckboxState,
                                true);
#endif
  registry->RegisterIntegerPref(kAutocompleteLastVersionRetentionPolicy, 0);
  registry->RegisterStringPref(kAutofillUploadEncodingSeed, "");
  registry->RegisterDictionaryPref(kAutofillVoteUploadEvents);
  registry->RegisterDictionaryPref(kAutofillMetadataUploadEvents);
  registry->RegisterTimePref(kAutofillUploadEventsLastResetTimestamp, {});
  registry->RegisterDictionaryPref(kAutofillSyncTransportOptIn);
#if BUILDFLAG(IS_ANDROID)
  // Automotive devices require stricter data protection for user privacy, so
  // mandatory reauth for autofill payment methods should always be enabled.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    registry->RegisterBooleanPref(kAutofillPaymentMethodsMandatoryReauth, true);
  } else {
    registry->RegisterBooleanPref(kAutofillPaymentMethodsMandatoryReauth,
                                  false);
  }
  registry->RegisterIntegerPref(
      kAutofillPaymentMethodsMandatoryReauthPromoShownCounter, 0);
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  registry->RegisterBooleanPref(kAutofillPaymentMethodsMandatoryReauth, false);
  registry->RegisterIntegerPref(
      kAutofillPaymentMethodsMandatoryReauthPromoShownCounter, 0);
#elif BUILDFLAG(IS_IOS)
  registry->RegisterBooleanPref(kAutofillPaymentMethodsMandatoryReauth, true);
#endif

  // Deprecated prefs registered for migration.
  registry->RegisterBooleanPref(kAutofillEnabledDeprecated, true);
  registry->RegisterBooleanPref(kAutofillOrphanRowsRemoved, false);
  registry->RegisterBooleanPref(kAutofillIbanEnabled, true);
  registry->RegisterIntegerPref(kAutofillLastVersionDisusedAddressesDeleted, 0);
  registry->RegisterIntegerPref(kAutofillLastVersionDisusedCreditCardsDeleted,
                                0);
  registry->RegisterStringPref(kAutofillAblationSeedPref, "");
  registry->RegisterBooleanPref(kAutofillRanQuasiDuplicateExtraDeduplication,
                                false);

#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(kAutofillUsingVirtualViewStructure, false);
  registry->RegisterBooleanPref(kAutofillThirdPartyPasswordManagersAllowed,
                                true);
  registry->RegisterBooleanPref(
      kFacilitatedPaymentsPix, /*default_value=*/true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kFacilitatedPaymentsEwallet, /*default_value=*/true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  registry->RegisterBooleanPref(
      kAutofillBnplEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kAutofillHasSeenBnpl, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
}

void MigrateDeprecatedAutofillPrefs(PrefService* pref_service) {
  // Added 09/2022.
  pref_service->ClearPref(kAutofillEnabledDeprecated);
  // Added 05/2023.
  pref_service->ClearPref(kAutofillOrphanRowsRemoved);
  // Added 09/2023.
  pref_service->ClearPref(kAutofillIbanEnabled);
  // Added 10/2023
  pref_service->ClearPref(kAutofillLastVersionDisusedAddressesDeleted);
  pref_service->ClearPref(kAutofillLastVersionDisusedCreditCardsDeleted);
  // Added 07/2024 (moved from profile pref to local state)
  pref_service->ClearPref(kAutofillAblationSeedPref);
  // Added 10/2024
  pref_service->ClearPref(kAutofillRanQuasiDuplicateExtraDeduplication);
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

void ClearSyncTransportOptIns(PrefService* prefs) {
  prefs->SetDict(kAutofillSyncTransportOptIn, base::Value::Dict());
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

void SetFacilitatedPaymentsEwallet(PrefService* prefs, bool value) {
#if BUILDFLAG(IS_ANDROID)
  prefs->SetBoolean(kFacilitatedPaymentsEwallet, value);
#endif  // BUILDFLAG(IS_ANDROID)
}

bool IsFacilitatedPaymentsEwalletEnabled(const PrefService* prefs) {
#if BUILDFLAG(IS_ANDROID)
  return prefs->GetBoolean(kFacilitatedPaymentsEwallet);
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
void SetAutofillBnplEnabled(PrefService* prefs, bool value) {
  prefs->SetBoolean(kAutofillBnplEnabled, value);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

bool IsAutofillBnplEnabled(const PrefService* prefs) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  return prefs->GetBoolean(kAutofillBnplEnabled);
#else
  return false;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// If called, always sets the pref to true, and once true, it will follow the
// user around forever.
void SetAutofillHasSeenBnpl(PrefService* prefs) {
  prefs->SetBoolean(kAutofillHasSeenBnpl, true);
}

bool HasSeenBnpl(const PrefService* prefs) {
  return prefs->GetBoolean(kAutofillHasSeenBnpl);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

}  // namespace autofill::prefs
