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
  kAddressEntry,
  kAutocompleteEntry,
  kInsecureContextPaymentDisabledMessage,
  kPasswordEntry,
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
    PopupItemId::kMerchantPromoCodeEntry};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_ITEM_IDS_H_
