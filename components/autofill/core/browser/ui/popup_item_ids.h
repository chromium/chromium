// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_ITEM_IDS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_ITEM_IDS_H_

#include "components/autofill/core/common/dense_set.h"

namespace autofill {

// This enum defines item identifiers for Autofill popup controller.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
enum class PopupItemId : int {
  // Autocomplete suggestions.
  kAutocompleteEntry,

  // Autofill profile suggestions.
  // Fill the whole for the current address. Triggered from the main/root
  // popup suggestion.
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
  kTitle,
  kEditAddressProfile,
  kDeleteAddressProfile,
  kAutofillOptions,

  // Compose suggestions.
  kCompose,

  // Datalist suggestions.
  kDatalistEntry,

  // Password suggestions.
  kPasswordEntry,
  kUsernameEntry,
  kAllSavedPasswordsEntry,
  kGeneratePasswordEntry,
  kShowAccountCards,
  kPasswordAccountStorageOptIn,
  kPasswordAccountStorageOptInAndGenerate,
  kAccountStoragePasswordEntry,
  kAccountStorageUsernameEntry,
  kPasswordAccountStorageReSignin,
  kPasswordAccountStorageEmpty,

  // Payment suggestions.
  kCreditCardEntry,
  kInsecureContextPaymentDisabledMessage,
  kScanCreditCard,
  kVirtualCreditCardEntry,
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
  kFieldByFieldFilling,
  kEntryNotSelectable,
  kSeparator,
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

// Set of `PopupItemId`s that trigger filling a value into an input element
// when the user selects a suggestion with that id.
inline constexpr auto kItemsTriggeringFieldFilling = DenseSet<PopupItemId>(
    {PopupItemId::kAccountStoragePasswordEntry,
     PopupItemId::kAccountStorageUsernameEntry, PopupItemId::kAddressEntry,
     PopupItemId::kAutocompleteEntry, PopupItemId::kCompose,
     PopupItemId::kCreditCardEntry, PopupItemId::kDatalistEntry,
     PopupItemId::kFillEverythingFromAddressProfile,
     PopupItemId::kMerchantPromoCodeEntry, PopupItemId::kPasswordEntry,
     PopupItemId::kUsernameEntry, PopupItemId::kVirtualCreditCardEntry});

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_ITEM_IDS_H_
