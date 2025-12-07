// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_prefs.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

namespace autofill::prefs {

namespace {
// To simplify the rollout of AutofillSilentlyRemoveQuasiDuplicates,
// deduplication can be run a second time per milestone for users enrolled in
// the experiment. This pref tracks whether deduplication was run a second time.
// TODO(crbug.com/325450676): Remove after the rollout finished.
constexpr char kAutofillRanQuasiDuplicateExtraDeduplication[] =
    "autofill.ran_quasi_duplicate_extra_deduplication";
}  // namespace

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // Synced prefs. Used for cross-device choices, e.g., credit card Autofill.
  registry->RegisterBooleanPref(
      kAutofillProfileEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      kAutofillLastVersionDeduped, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kAutofillAiIdentityEntitiesEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kAutofillAiSyncedOptInStatus, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      kAutofillAiLastVersionDeduped, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kAutofillAiTravelEntitiesEnabled, true,
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

  registry->RegisterStringPref(
      kAutofillNameAndEmailProfileSignature, "",
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      kAutofillNameAndEmailProfileNotSelectedCounter, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      kAutofillWasNameAndEmailProfileUsed, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);

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
  registry->RegisterDictionaryPref(
      kAutofillVoteSecondaryFormSignatureUploadEvents);
  registry->RegisterDictionaryPref(kAutofillMetadataUploadEvents);
  registry->RegisterTimePref(kAutofillUploadEventsLastResetTimestamp, {});
  registry->RegisterDictionaryPref(kAutofillSyncTransportOptIn);
  registry->RegisterBooleanPref(kAutofillRanExtraDeduplication, false);
#if BUILDFLAG(IS_ANDROID)
  // Automotive devices require stricter data protection for user privacy, so
  // mandatory reauth for autofill payment methods should always be enabled.
  if (base::android::device_info::is_automotive()) {
    registry->RegisterBooleanPref(kAutofillPaymentMethodsMandatoryReauth, true);
  } else {
    registry->RegisterBooleanPref(kAutofillPaymentMethodsMandatoryReauth,
                                  false);
  }
  registry->RegisterIntegerPref(
      kAutofillPaymentMethodsMandatoryReauthPromoShownCounter, 0);
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  registry->RegisterBooleanPref(kAutofillPaymentMethodsMandatoryReauth, false);
  registry->RegisterIntegerPref(
      kAutofillPaymentMethodsMandatoryReauthPromoShownCounter, 0);
#elif BUILDFLAG(IS_IOS)
  registry->RegisterBooleanPref(kAutofillPaymentMethodsMandatoryReauth, true);
#endif

  // Deprecated prefs registered for migration.
  registry->RegisterBooleanPref(kAutofillEnabledDeprecated, true);
  registry->RegisterStringPref(kAutofillAblationSeedPref, "");
  registry->RegisterBooleanPref(kAutofillRanQuasiDuplicateExtraDeduplication,
                                false);
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(kFacilitatedPaymentsPixAccountLinkingDeprecated,
                                /*default_value=*/true);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(kAutofillUsingVirtualViewStructure, false);
  registry->RegisterBooleanPref(kAutofillThirdPartyPasswordManagersAllowed,
                                true);
  registry->RegisterBooleanPref(
      kFacilitatedPaymentsEwallet, /*default_value=*/true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kFacilitatedPaymentsPix, /*default_value=*/true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  // The Pix account linking pref is a profile pref but not synced across
  // devices since users may prefer to have a different value for it on
  // different devices.
  registry->RegisterBooleanPref(kFacilitatedPaymentsPixAccountLinking,
                                /*default_value=*/true);
  registry->RegisterBooleanPref(
      kFacilitatedPaymentsA2AEnabled, /*default_value=*/true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kFacilitatedPaymentsA2ATriggeredOnce, /*default_value=*/false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(
      kAutofillBnplEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kAutofillHasSeenBnpl, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

  registry->RegisterBooleanPref(
      kAutofillAmountExtractionAiTermsSeen, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForHomeAndWork)) {
    registry->RegisterDictionaryPref(
        kAutofillHomeMetadata,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
    registry->RegisterDictionaryPref(
        kAutofillWorkMetadata,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
    registry->RegisterIntegerPref(kAutofillSilentUpdatesToHomeAddress, 0);
    registry->RegisterIntegerPref(kAutofillSilentUpdatesToWorkAddress, 0);
  }
}

void MigrateDeprecatedAutofillPrefs(PrefService* pref_service) {
  // Added 07/2024 (moved from profile pref to local state)
  pref_service->ClearPref(kAutofillAblationSeedPref);
  // Added 10/2024
  pref_service->ClearPref(kAutofillRanQuasiDuplicateExtraDeduplication);
  // Added 03/2025
  pref_service->ClearPref(kAutofillEnabledDeprecated);
#if BUILDFLAG(IS_ANDROID)
  // Added 08/2025
  pref_service->ClearPref(kFacilitatedPaymentsPixAccountLinkingDeprecated);
#endif  // BUILDFLAG(IS_ANDROID)
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
  if (prefs->GetBoolean(kAutofillProfileEnabled) == enabled) {
    return;
  }

  prefs->SetBoolean(kAutofillProfileEnabled, enabled);
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(AutofillAddressOptInChange)
  enum class AutofillAddressOptInChange {
    kOptIn = 0,
    kOptOut = 1,
    kMaxValue = kOptOut
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:AutofillAddressOptInChange)
  using enum AutofillAddressOptInChange;
  base::UmaHistogramEnumeration("Autofill.Address.IsEnabled.Change",
                                enabled ? kOptIn : kOptOut);
}

bool IsAutofillAiSyncedOptInStatusEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kAutofillAiSyncedOptInStatus);
}

void SetAutofillAiSyncedOptInStatus(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kAutofillAiSyncedOptInStatus, enabled);
}

bool IsPaymentMethodsMandatoryReauthEnabled(const PrefService* prefs) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_IOS)
  return prefs->GetBoolean(kAutofillPaymentMethodsMandatoryReauth);
#elif BUILDFLAG(IS_CHROMEOS)
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnablePaymentsMandatoryReauthChromeOs)) {
    return false;
  }
  return prefs->GetBoolean(kAutofillPaymentMethodsMandatoryReauth);
#else
  return false;
#endif
}

void SetPaymentMethodsMandatoryReauthEnabled(PrefService* prefs, bool enabled) {
#if BUILDFLAG(IS_ANDROID)
  // The user should not be able to update the pref value on automotive devices.
  CHECK(!base::android::device_info::is_automotive());
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_IOS)
  prefs->SetBoolean(kAutofillPaymentMethodsMandatoryReauth, enabled);
#elif BUILDFLAG(IS_CHROMEOS)
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnablePaymentsMandatoryReauthChromeOs)) {
    return;
  }
  prefs->SetBoolean(kAutofillPaymentMethodsMandatoryReauth, enabled);
#endif
}

bool IsPaymentMethodsMandatoryReauthSetExplicitly(const PrefService* prefs) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  return prefs->GetUserPrefValue(kAutofillPaymentMethodsMandatoryReauth) !=
         nullptr;
#elif BUILDFLAG(IS_CHROMEOS)
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnablePaymentsMandatoryReauthChromeOs)) {
    return false;
  }
  return prefs->GetUserPrefValue(kAutofillPaymentMethodsMandatoryReauth) !=
         nullptr;
#else
  return false;
#endif
}

bool IsPaymentMethodsMandatoryReauthPromoShownCounterBelowMaxCap(
    const PrefService* prefs) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  return prefs->GetInteger(
             kAutofillPaymentMethodsMandatoryReauthPromoShownCounter) <
         kMaxValueForMandatoryReauthPromoShownCounter;
#elif BUILDFLAG(IS_CHROMEOS)
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnablePaymentsMandatoryReauthChromeOs)) {
    return false;
  }
  return prefs->GetInteger(
             kAutofillPaymentMethodsMandatoryReauthPromoShownCounter) <
         kMaxValueForMandatoryReauthPromoShownCounter;
#else
  return false;
#endif
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
#elif BUILDFLAG(IS_CHROMEOS)
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnablePaymentsMandatoryReauthChromeOs)) {
    return;
  }

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
#endif
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

void SetFacilitatedPaymentsPixAccountLinking(PrefService* prefs, bool value) {
#if BUILDFLAG(IS_ANDROID)
  prefs->SetBoolean(kFacilitatedPaymentsPixAccountLinking, value);
#endif  // BUILDFLAG(IS_ANDROID)
}

bool IsFacilitatedPaymentsPixAccountLinkingEnabled(const PrefService* prefs) {
#if BUILDFLAG(IS_ANDROID)
  return prefs->GetBoolean(kFacilitatedPaymentsPixAccountLinking);
#else
  // Default to false on other platforms as the feature is Android-only.
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

bool IsFacilitatedPaymentsA2AEnabled(const PrefService* prefs) {
#if BUILDFLAG(IS_ANDROID)
  return prefs->GetBoolean(kFacilitatedPaymentsA2AEnabled);
#else
  // Default to false on other platforms as the feature is Android-only.
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

void SetFacilitatedPaymentsA2ATriggeredOnce(PrefService* prefs, bool value) {
#if BUILDFLAG(IS_ANDROID)
  prefs->SetBoolean(kFacilitatedPaymentsA2ATriggeredOnce, value);
#endif  // BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
void SetAutofillBnplEnabled(PrefService* prefs, bool value) {
  prefs->SetBoolean(kAutofillBnplEnabled, value);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

bool IsAutofillBnplEnabled(const PrefService* prefs) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  return prefs->GetBoolean(kAutofillBnplEnabled);
#else
  return false;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
// If called, always sets the pref to true, and once true, it will follow the
// user around forever.
void SetAutofillHasSeenBnpl(PrefService* prefs) {
  prefs->SetBoolean(kAutofillHasSeenBnpl, true);
}

bool HasSeenBnpl(const PrefService* prefs) {
  return prefs->GetBoolean(kAutofillHasSeenBnpl);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

// If called, always sets the pref to true, and once true, it will follow the
// user around forever.
void SetAutofillAmountExtractionAiTermsSeen(PrefService* prefs) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableAiBasedAmountExtraction)) {
    prefs->SetBoolean(kAutofillAmountExtractionAiTermsSeen, true);
  }
}

bool AmountExtractionAiTermsSeen(const PrefService* prefs) {
  return base::FeatureList::IsEnabled(
             features::kAutofillEnableAiBasedAmountExtraction) &&
         prefs->GetBoolean(kAutofillAmountExtractionAiTermsSeen);
}
}  // namespace autofill::prefs
