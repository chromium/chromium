// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_ITEM_IDS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_ITEM_IDS_H_

namespace autofill {

// This enum defines item identifiers for Autofill popup controller.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
enum PopupItemId {
  kAutocompleteEntry = 0,
  kInsecureContextPaymentDisabledMessage = -1,
  kPasswordEntry = -2,
  kSeparator = -3,
  kClearForm = -4,
  kAutofillOptions = -5,
  kDatalistEntry = -6,
  kScanCreditCard = -7,
  kTitle = -8,
  kCreditCardSigninPromo = -9,
  kUsernameEntry = -11,
  kAllSavedPasswordsEntry = -13,
  kGeneratePasswordEntry = -14,
  kShowAccountCards = -15,
  kPasswordAccountStorageOptIn = -16,
  kUseVirtualCard = -18,
  kPasswordAccountStorageOptInAndGenerate = -21,
  kAccountStoragePasswordEntry = -22,
  kAccountStorageUsernameEntry = -23,
  kPasswordAccountStorageReSignin = -24,
  kPasswordAccountStorageEmpty = -25,
  kMixedFormMessage = -26,
  kVirtualCreditCardEntry = -27,
  kWebauthnCredential = -28,
  kMerchantPromoCodeEntry = -29,
  kSeePromoCodeDetails = -30,
  kWebauthnSignInWithAnotherDevice = -31,
  kIbanEntry = -32,
};

// List of `PopupItemId` that trigger filling a value into an input element
// when the user selects the `PopupItemId`.
constexpr PopupItemId kItemsTriggeringFieldFilling[] = {
    PopupItemId::kAutocompleteEntry,
    PopupItemId::kPasswordEntry,
    PopupItemId::kDatalistEntry,
    PopupItemId::kUsernameEntry,
    PopupItemId::kAccountStoragePasswordEntry,
    PopupItemId::kAccountStorageUsernameEntry,
    PopupItemId::kVirtualCreditCardEntry,
    PopupItemId::kMerchantPromoCodeEntry};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_ITEM_IDS_H_
