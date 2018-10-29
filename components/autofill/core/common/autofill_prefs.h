// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PREFS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PREFS_H_

#include <string>

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace autofill {
namespace prefs {

// Alphabetical list of preference names specific to the Autofill
// component. Keep alphabetized, and document each in the .cc file.
extern const char kAutofillAcceptSaveCreditCardPromptState[];
extern const char kAutofillBillingCustomerNumber[];
// Do not get/set the value of this pref directly. Use provided getter/setter.
extern const char kAutofillCreditCardEnabled[];
extern const char kAutofillCreditCardSigninPromoImpressionCount[];
// Please use kAutofillCreditCardEnabled and kAutofillProfileEnabled instead.
extern const char kAutofillEnabledDeprecated[];
extern const char kAutofillJapanCityFieldMigrated[];
extern const char kAutofillLastVersionDeduped[];
extern const char kAutofillLastVersionDisusedAddressesDeleted[];
extern const char kAutofillLastVersionDisusedCreditCardsDeleted[];
extern const char kAutofillMigrateLocalCardsCancelledPrompt[];
extern const char kAutofillOrphanRowsRemoved[];
// Do not get/set the value of this pref directly. Use provided getter/setter.
extern const char kAutofillProfileEnabled[];
extern const char kAutofillProfileValidity[];
extern const char kAutofillUploadEvents[];
extern const char kAutofillUploadEventsLastResetTimestamp[];
extern const char kAutofillWalletImportEnabled[];
extern const char kAutofillWalletImportStorageCheckboxState[];

// Possible values for previous user decision when we displayed a save credit
// card prompt.
enum PreviousSaveCreditCardPromptUserDecision {
  PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_NONE,
  PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_ACCEPTED,
  PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_DENIED,
  NUM_PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISIONS
};

// Registers Autofill prefs.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Migrates deprecated Autofill prefs values.
void MigrateDeprecatedAutofillPrefs(PrefService* prefs);

bool IsAutocompleteEnabled(const PrefService* prefs);

bool IsAutofillEnabled(const PrefService* prefs);

void SetAutofillEnabled(PrefService* prefs, bool enabled);

bool IsCreditCardAutofillEnabled(const PrefService* prefs);

void SetCreditCardAutofillEnabled(PrefService* prefs, bool enabled);

bool IsAutofillManaged(const PrefService* prefs);

bool IsProfileAutofillManaged(const PrefService* prefs);

bool IsCreditCardAutofillManaged(const PrefService* prefs);

bool IsProfileAutofillEnabled(const PrefService* prefs);

void SetProfileAutofillEnabled(PrefService* prefs, bool enabled);

bool IsLocalCardMigrationPromptPreviouslyCancelled(const PrefService* prefs);

void SetLocalCardMigrationPromptPreviouslyCancelled(PrefService* prefs,
                                                    bool enabled);

bool IsPaymentsIntegrationEnabled(const PrefService* prefs);

void SetPaymentsIntegrationEnabled(PrefService* prefs, bool enabled);

std::string GetAllProfilesValidityMapsEncodedString(const PrefService* prefs);

}  // namespace prefs
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PREFS_H_
