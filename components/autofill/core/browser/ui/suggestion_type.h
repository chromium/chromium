// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_TYPE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_TYPE_H_

#include <ostream>

#include "components/autofill/core/common/dense_set.h"

namespace autofill {

// This enum defines item identifiers for Autofill suggestion controller.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
enum class SuggestionType {
  // Autocomplete suggestions.
  kAutocompleteEntry,

  // Autofill profile suggestions.
  // Fill the whole for the current address. On Desktop, it is triggered from the main (i.e. root popup) suggestion.
  kAddressEntry,
  // Fills all address related fields, e.g ADDRESS_HOME_LINE1,
  // ADDRESS_HOME_HOUSE_NUMBER etc.
  kFillFullAddress,
  // Fills all name related fields, e.g NAME_FIRST, NAME_MIDDLE, NAME_LAST
  // etc.
  kFillFullName,
  // Same as above, however it is triggered from the subpopup. This option
  // is displayed once the users is on group filling level or field by field
  // level. It is used as a way to allow users to go back to filling the whole
  // form. We need it as a separate id from `kAddressEntry` because it has a
  // different UI and for logging.
  kFillEverythingFromAddressProfile,
  // When triggered from a phone number field this suggestion will fill every
  // phone number field.
  kFillFullPhoneNumber,
  // Same as above, when triggered from an email address field this suggestion
  // will fill every email field.
  kFillFullEmail,
  kAddressFieldByFieldFilling,
  kEditAddressProfile,
  kDeleteAddressProfile,
  kAutofillOptions,

  // Compose suggestions.
  kCompose,
  kComposeDisable,
  kComposeGoToSettings,
  kComposeNeverShowOnThisSiteAgain,
  kComposeSavedStateNotification,

  // Datalist suggestions.
  kDatalistEntry,

  // Password suggestions.
  kPasswordEntry,
  kAllSavedPasswordsEntry,
  kGeneratePasswordEntry,
  kShowAccountCards,
  kPasswordAccountStorageOptIn,
  kPasswordAccountStorageOptInAndGenerate,
  kAccountStoragePasswordEntry,
  kPasswordAccountStorageReSignin,
  kPasswordAccountStorageEmpty,
  kPasswordFieldByFieldFilling,
  kFillPassword,
  kViewPasswordDetails,

  // Payment suggestions.
  kCreditCardEntry,
  kInsecureContextPaymentDisabledMessage,
  kScanCreditCard,
  kVirtualCreditCardEntry,
  kCreditCardFieldByFieldFilling,
  kIbanEntry,

  // Plus address suggestions.
  kCreateNewPlusAddress,
  kFillExistingPlusAddress,

  // Promotion suggestions.
  kMerchantPromoCodeEntry,
  kSeePromoCodeDetails,

  // Webauthn suggestions.
  kWebauthnCredential,
  kWebauthnSignInWithAnotherDevice,

  // Other suggestions.
  kTitle,
  kSeparator,
  // TODO(b/40266549): Rename to Undo.
  kClearForm,
  kMixedFormMessage,

  // Top level suggestion rendered when test addresses are available. Shown only
  // when DevTools is open.
  kDevtoolsTestAddresses,
  // Test address option that specifies a full address for a country
  // so that users can test their form with it.
  kDevtoolsTestAddressEntry,

  kMaxValue = kDevtoolsTestAddressEntry
};

std::ostream& operator<<(std::ostream& os, SuggestionType type);

// Set of `SuggestionType`s that trigger filling a value into an input element
// when the user selects a suggestion with that id.
inline constexpr auto kItemsTriggeringFieldFilling = DenseSet(
    {SuggestionType::kAccountStoragePasswordEntry, SuggestionType::kAddressEntry,
     SuggestionType::kAutocompleteEntry, SuggestionType::kCompose,
     SuggestionType::kCreditCardEntry, SuggestionType::kDatalistEntry,
     SuggestionType::kFillEverythingFromAddressProfile,
     SuggestionType::kMerchantPromoCodeEntry, SuggestionType::kPasswordEntry,
     SuggestionType::kVirtualCreditCardEntry});

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_TYPE_H_
