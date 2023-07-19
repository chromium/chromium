// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_ITEM_IDS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_ITEM_IDS_H_

namespace autofill {

// This enum defines item identifiers for Autofill popup controller.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
enum class PopupItemId : int {
  kCreditCardEntry,
  // Fill the whole for the current address. Triggered from the main/root popup
  // suggestion.
  kAddressEntry,
  // Fills all address related fields, e.g ADDRESS_HOME_LINE1,
  // ADDRESS_HOME_HOUSE_NUMBER etc.
  kFillFullAddress,
  // Fills all name related fields, e.g NAME_FIRST, NAME_MIDDLE, NAME_LAST etc.
  kFillFullName,
  // Same as above, however it is triggered from the subpopup. This option
  // is displayed once the users is on group filling level or field by field
  // level. It is used as a way to allow users to go back to filling the whole
  // form. We need it as a separate id from `kAddressEntry` because it has a
  // different UI and for logging.
  kFillEverythingFromAddressProfile,
  kAutocompleteEntry,
  kInsecureContextPaymentDisabledMessage,
  kPasswordEntry,
  kFieldByFieldFilling,
  kSeparator,
  kClearForm,
  kAutofillOptions,
  kDatalistEntry,
  kScanCreditCard,
  kTitle,
  kUsernameEntry,
  kAllSavedPasswordsEntry,
  kGeneratePasswordEntry,
  kShowAccountCards,
  kPasswordAccountStorageOptIn,
  kUseVirtualCard,
  kPasswordAccountStorageOptInAndGenerate,
  kAccountStoragePasswordEntry,
  kAccountStorageUsernameEntry,
  kPasswordAccountStorageReSignin,
  kPasswordAccountStorageEmpty,
  kMixedFormMessage,
  kVirtualCreditCardEntry,
  kWebauthnCredential,
  kMerchantPromoCodeEntry,
  kSeePromoCodeDetails,
  kWebauthnSignInWithAnotherDevice,
  kIbanEntry,
  kDeleteAddressProfile,
};

// List of `PopupItemId` that trigger filling a value into an input element
// when the user selects the `PopupItemId`.
constexpr PopupItemId kItemsTriggeringFieldFilling[] = {
    PopupItemId::kAutocompleteEntry,
    PopupItemId::kAddressEntry,
    PopupItemId::kCreditCardEntry,
    PopupItemId::kPasswordEntry,
    PopupItemId::kDatalistEntry,
    PopupItemId::kUsernameEntry,
    PopupItemId::kAccountStoragePasswordEntry,
    PopupItemId::kAccountStorageUsernameEntry,
    PopupItemId::kVirtualCreditCardEntry,
    PopupItemId::kMerchantPromoCodeEntry,
    PopupItemId::kFillEverythingFromAddressProfile};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_ITEM_IDS_H_
