// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PREFS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PREFS_H_

#include <string>

#include "build/build_config.h"
#include "google_apis/gaia/core_account_id.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace autofill {
namespace prefs {

// Alphabetical list of preference names specific to the Autofill
// component. Keep alphabetized, and document each in the .cc file.
// Do not get/set the value of this pref directly. Use provided getter/setter.
extern const char kAutofillCreditCardEnabled[];
extern const char kAutofillCreditCardFidoAuthEnabled[];
#if BUILDFLAG(IS_ANDROID)
extern const char kAutofillCreditCardFidoAuthOfferCheckboxState[];
#endif
// Please use kAutofillCreditCardEnabled, kAutofillIBANEnabled and
// kAutofillProfileEnabled instead.
extern const char kAutofillEnabledDeprecated[];
extern const char kAutofillHasSeenIban[];
extern const char kAutofillIBANEnabled[];
extern const char kAutofillLastVersionDeduped[];
extern const char kAutofillLastVersionDisusedAddressesDeleted[];
extern const char kAutofillLastVersionDisusedCreditCardsDeleted[];
extern const char kAutofillOrphanRowsRemoved[];
extern const char kAutofillPaymentCvcStorageAndFilling[];
// Do not get/set the value of this pref directly. Use provided getter/setter.
extern const char kAutofillProfileEnabled[];
extern const char kAutofillSyncTransportOptIn[];
extern const char kAutofillStatesDataDir[];
extern const char kAutofillUploadEncodingSeed[];
extern const char kAutofillUploadEvents[];
extern const char kAutofillUploadEventsLastResetTimestamp[];
extern const char kAutofillWalletImportEnabled[];
extern const char kAutocompleteLastVersionRetentionPolicy[];
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
extern const char kAutofillPaymentMethodsMandatoryReauth[];
extern const char kAutofillPaymentMethodsMandatoryReauthPromoShownCounter[];
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

// The maximum value for the
// `kAutofillPaymentMethodsMandatoryReauthPromoShownCounter` pref. If this
// value is reached, we should not show a mandatory re-auth promo.
const int kMaxValueForMandatoryReauthPromoShownCounter = 2;

namespace sync_transport_opt_in {
enum Flags {
  kWallet = 1 << 0,
};
}  // namespace sync_transport_opt_in

// Registers Autofill prefs.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Migrates deprecated Autofill prefs values.
void MigrateDeprecatedAutofillPrefs(PrefService* prefs);

bool IsAutocompleteEnabled(const PrefService* prefs);

bool IsCreditCardFIDOAuthEnabled(PrefService* prefs);

void SetCreditCardFIDOAuthEnabled(PrefService* prefs, bool enabled);

bool IsAutofillCreditCardEnabled(const PrefService* prefs);

void SetAutofillCreditCardEnabled(PrefService* prefs, bool enabled);

bool HasSeenIban(const PrefService* prefs);

void SetAutofillHasSeenIban(PrefService* prefs);

bool IsAutofillIBANEnabled(const PrefService* prefs);

void SetAutofillIBANEnabled(PrefService* prefs, bool enabled);

bool IsAutofillManaged(const PrefService* prefs);

bool IsAutofillProfileManaged(const PrefService* prefs);

bool IsAutofillCreditCardManaged(const PrefService* prefs);

bool IsAutofillProfileEnabled(const PrefService* prefs);

void SetAutofillProfileEnabled(PrefService* prefs, bool enabled);

bool IsPaymentsIntegrationEnabled(const PrefService* prefs);

void SetPaymentsIntegrationEnabled(PrefService* prefs, bool enabled);

bool IsPaymentMethodsMandatoryReauthEnabled(const PrefService* prefs);

void SetPaymentMethodsMandatoryReauthEnabled(PrefService* prefs, bool enabled);

bool ShouldShowPaymentMethodsMandatoryReauthPromo(const PrefService* prefs);

void IncrementPaymentMethodsMandatoryReauthPromoShownCounter(
    PrefService* prefs);

bool IsPaymentCvcStorageAndFillingEnabled(const PrefService* prefs);

void SetPaymentCvcStorageAndFilling(PrefService* prefs, bool value);

void SetUserOptedInWalletSyncTransport(PrefService* prefs,
                                       const CoreAccountId& account_id,
                                       bool opted_in);

bool IsUserOptedInWalletSyncTransport(const PrefService* prefs,
                                      const CoreAccountId& account_id);

void ClearSyncTransportOptIns(PrefService* prefs);

}  // namespace prefs
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PREFS_H_
