// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_TYPE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_TYPE_H_

#include <ostream>

namespace autofill {

// This enum defines item identifiers for Autofill suggestion controller.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// To add a new entry `kSampleEntry` add it to the appropriate section of the
// enum (not necessarily at the end). Set its value to the current `kMaxValue`
// and increase `kMaxValue` by 1.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
//
// LINT.IfChange(SuggestionType)
enum class SuggestionType {
  // Autocomplete suggestions.
  kAutocompleteEntry = 0,

  // Autofill profile suggestions.
  // Fill the whole for the current address. On Desktop, it is triggered from
  // the main (i.e. root popup) suggestion.
  kAddressEntry = 1,
  // kFillFullAddress = 2, // DEPRECATED
  // kFillFullName = 3, // DEPRECATED
  // kFillEverythingFromAddressProfile = 4, // DEPRECATED
  // kFillFullPhoneNumber = 5, // DEPRECATED
  // kFillFullEmail = 6, // DEPRECATED
  kAddressFieldByFieldFilling = 7,
  // kEditAddressProfile = 8, // DEPRECATED
  // kDeleteAddressProfile = 9, // DEPRECATED
  // Display a suggestion containing address profile data based on prefix
  // matching, regardless of the type of the field.
  kAddressEntryOnTyping = 63,

  kManageAddress = 10,
  kManageAutofillAi = 64,
  kManageCreditCard = 11,
  kManageIban = 12,
  kManagePlusAddress = 13,
  kManageLoyaltyCard = 68,

  // Compose popup suggestion shown when no Compose session exists.
  kComposeProactiveNudge = 14,
  // Compose popup suggestion shown when there is an existing Compose session.
  kComposeResumeNudge = 15,
  // Compose popup suggestion shown after the Compose dialog closes.
  kComposeSavedStateNotification = 16,
  // Compose sub-menu suggestions
  kComposeDisable = 17,
  kComposeGoToSettings = 18,
  kComposeNeverShowOnThisSiteAgain = 19,

  // Datalist suggestions.
  kDatalistEntry = 20,

  // Password related suggestion. Fills a password credential.
  kPasswordEntry = 21,
  // Password related suggestion. Opens management UI for passwords and/or
  // passkeys.
  kAllSavedPasswordsEntry = 22,
  // Password related suggestion. Generates a password for the field.
  kGeneratePasswordEntry = 23,
  // Password related suggestion. Displays the option to enable credentials from
  // the account storage.
  // Deprecated: kPasswordAccountStorageOptIn = 25,
  // Password related suggestion. Displays the option to enable password
  // generation and saving in the account storage.
  // Deprecated: kPasswordAccountStorageOptInAndGenerate = 26,
  // Password related suggestion. Displays a password from the account storage.
  kAccountStoragePasswordEntry = 27,
  // Password related suggestion. Displays the option to re-signin to enable the
  // account storage.
  // Deprecated: kPasswordAccountStorageReSignin = 28,
  // Password related suggestion. Displays that there is no fillable credentials
  // after opting into the account storage.
  // Deprecated: kPasswordAccountStorageEmpty = 29,
  // Password sub-popup suggestion. Fills the username from the manual fallback
  // entry.
  kPasswordFieldByFieldFilling = 30,
  // Password sub-popup suggestion. Fills the password from the manual fallback
  // entry.
  kFillPassword = 31,
  // Password sub-popup suggestion. Triggers the password details view from the
  // manual fallback entry.
  kViewPasswordDetails = 32,

  // Payment suggestions.
  // kShowAccountCards = 24, // DEPRECATED
  kCreditCardEntry = 33,
  kInsecureContextPaymentDisabledMessage = 34,
  kScanCreditCard = 35,
  kVirtualCreditCardEntry = 36,
  // kCreditCardFieldByFieldFilling = 37, // DEPRECATED
  kIbanEntry = 38,
  kBnplEntry = 61,
  kSaveAndFillCreditCardEntry = 62,

  // Plus address suggestions.
  kCreateNewPlusAddress = 39,
  kCreateNewPlusAddressInline = 52,
  kFillExistingPlusAddress = 40,
  kPlusAddressError = 57,

  // Promotion suggestions.
  kMerchantPromoCodeEntry = 41,
  kSeePromoCodeDetails = 42,

  // Federated profiles suggestions.
  kIdentityCredential = 66,

  // Loyalty card suggestions.
  kLoyaltyCardEntry = 67,

  // Home & Work suggestions.
  kHomeAndWorkAddressEntry = 69,

  // Webauthn suggestions.
  kWebauthnCredential = 43,
  kWebauthnSignInWithAnotherDevice = 44,

  // Other suggestions.
  kTitle = 45,
  kSeparator = 46,
  // TODO(crbug.com/40266549): Rename to Undo once iOS implements it - it still
  // works as clear form there.
  kUndoOrClear = 47,
  kMixedFormMessage = 48,

  // Top level suggestion rendered when test addresses are available. Shown only
  // when DevTools is open.
  kDevtoolsTestAddresses = 49,
  // Test address option that specifies a full address for a country
  // so that users can test their form with it.
  kDevtoolsTestAddressEntry = 50,
  // Test address option that gives users feedback about what the
  // suggestions with country names as main text mean.
  kDevtoolsTestAddressByCountry = 51,

  // kRetrieveAutofillAi = 53, // DEPRECATED
  // kAutofillAiLoadingState = 54, // DEPRECATED
  // Autofill AI filling suggestion.
  kFillAutofillAi = 55,
  // kAutofillAiFeedback = 56, // DEPRECATED
  // kPredictionImprovementsDetails = 58, // DEPRECATED
  // kAutofillAiError = 59, // DEPRECATED
  // kEditAutofillAiData = 60, // DEPRECATED

  // kPendingStateSignin suggestion is displayed when the user is in the pending
  // state. On click the user will be directed to sign in.
  kPendingStateSignin = 65,

  // Next ID: 70
  kMaxValue = kHomeAndWorkAddressEntry
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:SuggestionType)

std::string_view SuggestionTypeToStringView(SuggestionType type);
std::string SuggestionTypeToString(SuggestionType type);

std::ostream& operator<<(std::ostream& os, SuggestionType type);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_TYPE_H_
