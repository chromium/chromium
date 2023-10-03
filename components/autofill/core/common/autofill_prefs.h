// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PREFS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PREFS_H_

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
#endif  // BUILDFLAG(IS_ANDROID)
// Please use kAutofillCreditCardEnabled and kAutofillProfileEnabled instead.
extern const char kAutofillEnabledDeprecated[];
extern const char kAutofillHasSeenIban[];
extern const char kAutofillIbanEnabled[];
extern const char kAutofillLastVersionDeduped[];
extern const char kAutofillLastVersionDisusedAddressesDeleted[];
extern const char kAutofillLastVersionDisusedCreditCardsDeleted[];
extern const char kAutofillOrphanRowsRemoved[];
extern const char kAutofillPaymentCvcStorage[];
// Do not get/set the value of this pref directly. Use provided getter/setter.
extern const char kAutofillProfileEnabled[];
extern const char kAutofillSyncTransportOptIn[];
extern const char kAutofillStatesDataDir[];
extern const char kAutofillUploadEncodingSeed[];
extern const char kAutofillUploadEvents[];
extern const char kAutofillUploadEventsLastResetTimestamp[];
extern const char kAutocompleteLastVersionRetentionPolicy[];
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_IOS)
extern const char kAutofillPaymentMethodsMandatoryReauth[];
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_IOS)
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
extern const char kAutofillPaymentMethodsMandatoryReauthPromoShownCounter[];
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_ANDROID)
extern const char kAutofillUsingVirtualViewStructure[];
#endif  // BUILDFLAG(IS_ANDROID)

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

bool IsAutofillPaymentMethodsEnabled(const PrefService* prefs);

void SetAutofillPaymentMethodsEnabled(PrefService* prefs, bool enabled);

bool HasSeenIban(const PrefService* prefs);

void SetAutofillHasSeenIban(PrefService* prefs);

bool IsAutofillIbanEnabled(const PrefService* prefs);

void SetAutofillIbanEnabled(PrefService* prefs, bool enabled);

bool IsAutofillManaged(const PrefService* prefs);

bool IsAutofillProfileManaged(const PrefService* prefs);

bool IsAutofillCreditCardManaged(const PrefService* prefs);

bool IsAutofillProfileEnabled(const PrefService* prefs);

void SetAutofillProfileEnabled(PrefService* prefs, bool enabled);

bool IsPaymentMethodsMandatoryReauthEnabled(const PrefService* prefs);

// Returns true if the user has ever made an explicit decision for
// this pref. Note that this function returns whether the user has set the pref,
// not the value of the pref itself.
bool IsPaymentMethodsMandatoryReauthSetExplicitly(const PrefService* prefs);

void SetPaymentMethodsMandatoryReauthEnabled(PrefService* prefs, bool enabled);

bool IsPaymentMethodsMandatoryReauthPromoShownCounterBelowMaxCap(
    const PrefService* prefs);

void IncrementPaymentMethodsMandatoryReauthPromoShownCounter(
    PrefService* prefs);

bool IsPaymentCvcStorageEnabled(const PrefService* prefs);

void SetPaymentCvcStorage(PrefService* prefs, bool value);

void SetUserOptedInWalletSyncTransport(PrefService* prefs,
                                       const CoreAccountId& account_id,
                                       bool opted_in);

bool IsUserOptedInWalletSyncTransport(const PrefService* prefs,
                                      const CoreAccountId& account_id);

void ClearSyncTransportOptIns(PrefService* prefs);

bool UsesVirtualViewStructureForAutofill(const PrefService* prefs);

}  // namespace prefs
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PREFS_H_
