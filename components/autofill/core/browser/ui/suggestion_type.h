// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_TYPE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_TYPE_H_

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
// Keep this enum up to date with the one in
// tools/metrics/histograms/metadata/autofill/enums.xml.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
enum class SuggestionType {
  // Autocomplete suggestions.
  kAutocompleteEntry = 0,

  // Autofill profile suggestions.
  // Fill the whole for the current address. On Desktop, it is triggered from
  // the main (i.e. root popup) suggestion.
  kAddressEntry = 1,
  // Fills all address related fields, e.g ADDRESS_HOME_LINE1,
  // ADDRESS_HOME_HOUSE_NUMBER etc.
  kFillFullAddress = 2,
  // Fills all name related fields, e.g NAME_FIRST, NAME_MIDDLE, NAME_LAST
  // etc.
  kFillFullName = 3,
  // Same as above, however it is triggered from the subpopup. This option
  // is displayed once the users is on group filling level or field by field
  // level. It is used as a way to allow users to go back to filling the whole
  // form. We need it as a separate id from `kAddressEntry` because it has a
  // different UI and for logging.
  kFillEverythingFromAddressProfile = 4,
  // When triggered from a phone number field this suggestion will fill every
  // phone number field.
  kFillFullPhoneNumber = 5,
  // Same as above, when triggered from an email address field this suggestion
  // will fill every email field.
  kFillFullEmail = 6,
  kAddressFieldByFieldFilling = 7,
  kEditAddressProfile = 8,
  kDeleteAddressProfile = 9,
  kManageAddress = 10,
  kManageCreditCard = 11,
  kManageIban = 12,
  kManagePlusAddress = 13,

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
  kPasswordAccountStorageOptIn = 25,
  // Password related suggestion. Displays the option to enable password
  // generation and saving in the account storage.
  kPasswordAccountStorageOptInAndGenerate = 26,
  // Password related suggestion. Displays a password from the account storage.
  kAccountStoragePasswordEntry = 27,
  // Password related suggestion. Displays the option to re-signin to enable the
  // account storage.
  kPasswordAccountStorageReSignin = 28,
  // Password related suggestion. Displays that there is no fillable credentials
  // after opting into the account storage.
  kPasswordAccountStorageEmpty = 29,
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
  kShowAccountCards = 24,
  kCreditCardEntry = 33,
  kInsecureContextPaymentDisabledMessage = 34,
  kScanCreditCard = 35,
  kVirtualCreditCardEntry = 36,
  kCreditCardFieldByFieldFilling = 37,
  kIbanEntry = 38,

  // Plus address suggestions.
  kCreateNewPlusAddress = 39,
  kCreateNewPlusAddressInline = 52,
  kFillExistingPlusAddress = 40,
  kPlusAddressError = 57,

  // Promotion suggestions.
  kMerchantPromoCodeEntry = 41,
  kSeePromoCodeDetails = 42,

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

  // Trigger for retrieving prediction improvements. Part of the footer.
  kRetrievePredictionImprovements = 53,
  // Loading indicator shown while retrieving prediction improvements.
  kPredictionImprovementsLoadingState = 54,
  // Fill prediction improvements.
  kFillPredictionImprovements = 55,
  // Suggestion that provides users the possibility to give feedback about
  // predictions improvements.
  kPredictionImprovementsFeedback = 56,
  // DEPRECATED due to UX changes.
  // Allows the users to navigate to more details about improved predictions.
  // kPredictionImprovementsDetails = 58,
  // Suggestion displayed when there is an error with prediction improvements.
  kPredictionImprovementsError = 59,
  // Suggestion displayed when there is an error with prediction improvements.
  kEditPredictionImprovementsInformation = 60,

  kMaxValue = kEditPredictionImprovementsInformation
};

std::string_view SuggestionTypeToStringView(SuggestionType type);
std::string SuggestionTypeToString(SuggestionType type);

std::ostream& operator<<(std::ostream& os, SuggestionType type);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_TYPE_H_
