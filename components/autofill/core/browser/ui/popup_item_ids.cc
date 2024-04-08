// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/popup_item_ids.h"

namespace autofill {

std::ostream& operator<<(std::ostream& os, PopupItemId popup_item_id) {
  switch (popup_item_id) {
    case PopupItemId::kAutocompleteEntry:
      os << "kAutocompleteEntry";
      break;
    case PopupItemId::kAddressEntry:
      os << "kAddressEntry";
      break;
    case PopupItemId::kFillFullAddress:
      os << "kFillFullAddress";
      break;
    case PopupItemId::kFillFullName:
      os << "kFillFullName";
      break;
    case PopupItemId::kFillEverythingFromAddressProfile:
      os << "kFillEverythingFromAddressProfile";
      break;
    case PopupItemId::kFillFullPhoneNumber:
      os << "kFillFullPhoneNumber";
      break;
    case PopupItemId::kFillFullEmail:
      os << "kFillFullEmail";
      break;
    case PopupItemId::kAddressFieldByFieldFilling:
      os << "kAddressFieldByFieldFilling";
      break;
    case PopupItemId::kEditAddressProfile:
      os << "kEditAddressProfile";
      break;
    case PopupItemId::kDeleteAddressProfile:
      os << "kDeleteAddressProfile";
      break;
    case PopupItemId::kAutofillOptions:
      os << "kAutofillOptions";
      break;
    case PopupItemId::kCompose:
      os << "kCompose";
      break;
    case PopupItemId::kComposeSavedStateNotification:
      os << "kComposeSavedStateNotification";
      break;
    case PopupItemId::kDatalistEntry:
      os << "kDatalistEntry";
      break;
    case PopupItemId::kPasswordEntry:
      os << "kPasswordEntry";
      break;
    case PopupItemId::kAllSavedPasswordsEntry:
      os << "kAllSavedPasswordsEntry";
      break;
    case PopupItemId::kGeneratePasswordEntry:
      os << "kGeneratePasswordEntry";
      break;
    case PopupItemId::kShowAccountCards:
      os << "kShowAccountCards";
      break;
    case PopupItemId::kPasswordAccountStorageOptIn:
      os << "kPasswordAccountStorageOptIn";
      break;
    case PopupItemId::kPasswordAccountStorageOptInAndGenerate:
      os << "kPasswordAccountStorageOptInAndGenerate";
      break;
    case PopupItemId::kAccountStoragePasswordEntry:
      os << "kAccountStoragePasswordEntry";
      break;
    case PopupItemId::kPasswordAccountStorageReSignin:
      os << "kPasswordAccountStorageReSignin";
      break;
    case PopupItemId::kPasswordAccountStorageEmpty:
      os << "kPasswordAccountStorageEmpty";
      break;
    case PopupItemId::kPasswordFieldByFieldFilling:
      os << "kPasswordFieldByFieldFilling";
      break;
    case PopupItemId::kFillPassword:
      os << "kFillPassword";
      break;
    case PopupItemId::kViewPasswordDetails:
      os << "kViewPasswordDetails";
      break;
    case PopupItemId::kCreditCardEntry:
      os << "kCreditCardEntry";
      break;
    case PopupItemId::kInsecureContextPaymentDisabledMessage:
      os << "kInsecureContextPaymentDisabledMessage";
      break;
    case PopupItemId::kScanCreditCard:
      os << "kScanCreditCard";
      break;
    case PopupItemId::kVirtualCreditCardEntry:
      os << "kVirtualCreditCardEntry";
      break;
    case PopupItemId::kCreditCardFieldByFieldFilling:
      os << "kCreditCardFieldByFieldFilling";
      break;
    case PopupItemId::kIbanEntry:
      os << "kIbanEntry";
      break;
    case PopupItemId::kCreateNewPlusAddress:
      os << "kCreateNewPlusAddress";
      break;
    case PopupItemId::kFillExistingPlusAddress:
      os << "kFillExistingPlusAddress";
      break;
    case PopupItemId::kMerchantPromoCodeEntry:
      os << "kMerchantPromoCodeEntry";
      break;
    case PopupItemId::kSeePromoCodeDetails:
      os << "kSeePromoCodeDetails";
      break;
    case PopupItemId::kWebauthnCredential:
      os << "kWebauthnCredential";
      break;
    case PopupItemId::kWebauthnSignInWithAnotherDevice:
      os << "kWebauthnSignInWithAnotherDevice";
      break;
    case PopupItemId::kTitle:
      os << "kTitle";
      break;
    case PopupItemId::kSeparator:
      os << "kSeparator";
      break;
    case PopupItemId::kClearForm:
      os << "kClearForm";
      break;
    case PopupItemId::kMixedFormMessage:
      os << "kMixedFormMessage";
      break;
    case PopupItemId::kDevtoolsTestAddresses:
      os << "kDevtoolsTestAddresses";
      break;
    case PopupItemId::kDevtoolsTestAddressEntry:
      os << "kDevtoolsTestAddressEntry";
      break;
  }
  return os;
}

}  // namespace autofill
