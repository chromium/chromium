// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/suggestion_type.h"

namespace autofill {

std::ostream& operator<<(std::ostream& os, SuggestionType type) {
  switch (type) {
    case SuggestionType::kAutocompleteEntry:
      os << "kAutocompleteEntry";
      break;
    case SuggestionType::kAddressEntry:
      os << "kAddressEntry";
      break;
    case SuggestionType::kFillFullAddress:
      os << "kFillFullAddress";
      break;
    case SuggestionType::kFillFullName:
      os << "kFillFullName";
      break;
    case SuggestionType::kFillEverythingFromAddressProfile:
      os << "kFillEverythingFromAddressProfile";
      break;
    case SuggestionType::kFillFullPhoneNumber:
      os << "kFillFullPhoneNumber";
      break;
    case SuggestionType::kFillFullEmail:
      os << "kFillFullEmail";
      break;
    case SuggestionType::kAddressFieldByFieldFilling:
      os << "kAddressFieldByFieldFilling";
      break;
    case SuggestionType::kEditAddressProfile:
      os << "kEditAddressProfile";
      break;
    case SuggestionType::kDeleteAddressProfile:
      os << "kDeleteAddressProfile";
      break;
    case SuggestionType::kAutofillOptions:
      os << "kAutofillOptions";
      break;
    case SuggestionType::kCompose:
      os << "kCompose";
      break;
    case SuggestionType::kComposeDisable:
      os << "kComposeDisable";
      break;
    case SuggestionType::kComposeGoToSettings:
      os << "kComposeGoToSettings";
      break;
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
      os << "kComposeNeverShowOnThisSiteAgain";
      break;
    case SuggestionType::kComposeSavedStateNotification:
      os << "kComposeSavedStateNotification";
      break;
    case SuggestionType::kDatalistEntry:
      os << "kDatalistEntry";
      break;
    case SuggestionType::kPasswordEntry:
      os << "kPasswordEntry";
      break;
    case SuggestionType::kAllSavedPasswordsEntry:
      os << "kAllSavedPasswordsEntry";
      break;
    case SuggestionType::kGeneratePasswordEntry:
      os << "kGeneratePasswordEntry";
      break;
    case SuggestionType::kShowAccountCards:
      os << "kShowAccountCards";
      break;
    case SuggestionType::kPasswordAccountStorageOptIn:
      os << "kPasswordAccountStorageOptIn";
      break;
    case SuggestionType::kPasswordAccountStorageOptInAndGenerate:
      os << "kPasswordAccountStorageOptInAndGenerate";
      break;
    case SuggestionType::kAccountStoragePasswordEntry:
      os << "kAccountStoragePasswordEntry";
      break;
    case SuggestionType::kPasswordAccountStorageReSignin:
      os << "kPasswordAccountStorageReSignin";
      break;
    case SuggestionType::kPasswordAccountStorageEmpty:
      os << "kPasswordAccountStorageEmpty";
      break;
    case SuggestionType::kPasswordFieldByFieldFilling:
      os << "kPasswordFieldByFieldFilling";
      break;
    case SuggestionType::kFillPassword:
      os << "kFillPassword";
      break;
    case SuggestionType::kViewPasswordDetails:
      os << "kViewPasswordDetails";
      break;
    case SuggestionType::kCreditCardEntry:
      os << "kCreditCardEntry";
      break;
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
      os << "kInsecureContextPaymentDisabledMessage";
      break;
    case SuggestionType::kScanCreditCard:
      os << "kScanCreditCard";
      break;
    case SuggestionType::kVirtualCreditCardEntry:
      os << "kVirtualCreditCardEntry";
      break;
    case SuggestionType::kCreditCardFieldByFieldFilling:
      os << "kCreditCardFieldByFieldFilling";
      break;
    case SuggestionType::kIbanEntry:
      os << "kIbanEntry";
      break;
    case SuggestionType::kCreateNewPlusAddress:
      os << "kCreateNewPlusAddress";
      break;
    case SuggestionType::kFillExistingPlusAddress:
      os << "kFillExistingPlusAddress";
      break;
    case SuggestionType::kMerchantPromoCodeEntry:
      os << "kMerchantPromoCodeEntry";
      break;
    case SuggestionType::kSeePromoCodeDetails:
      os << "kSeePromoCodeDetails";
      break;
    case SuggestionType::kWebauthnCredential:
      os << "kWebauthnCredential";
      break;
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
      os << "kWebauthnSignInWithAnotherDevice";
      break;
    case SuggestionType::kTitle:
      os << "kTitle";
      break;
    case SuggestionType::kSeparator:
      os << "kSeparator";
      break;
    case SuggestionType::kClearForm:
      os << "kClearForm";
      break;
    case SuggestionType::kMixedFormMessage:
      os << "kMixedFormMessage";
      break;
    case SuggestionType::kDevtoolsTestAddresses:
      os << "kDevtoolsTestAddresses";
      break;
    case SuggestionType::kDevtoolsTestAddressEntry:
      os << "kDevtoolsTestAddressEntry";
      break;
  }
  return os;
}

}  // namespace autofill
