// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/suggestion_type.h"

#include "base/notreached.h"

namespace autofill {

std::string_view SuggestionTypeToStringView(SuggestionType type) {
  switch (type) {
    case SuggestionType::kAutocompleteEntry:
      return "kAutocompleteEntry";
    case SuggestionType::kAddressEntry:
      return "kAddressEntry";
    case SuggestionType::kAddressEntryOnTyping:
      return "kAddressEntryOnTyping";
    case SuggestionType::kAddressFieldByFieldFilling:
      return "kAddressFieldByFieldFilling";
    case SuggestionType::kManageAddress:
      return "kManageAddress";
    case SuggestionType::kManageAutofillAi:
      return "kManageAutofillAi";
    case SuggestionType::kManageCreditCard:
      return "kManageCreditCard";
    case SuggestionType::kManageIban:
      return "kManageIban";
    case SuggestionType::kManagePlusAddress:
      return "kManagePlusAddress";
    case SuggestionType::kManageLoyaltyCard:
      return "kManageLoyaltyCard";
    case SuggestionType::kComposeResumeNudge:
      return "kComposeResumeNudge";
    case SuggestionType::kComposeDisable:
      return "kComposeDisable";
    case SuggestionType::kComposeGoToSettings:
      return "kComposeGoToSettings";
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
      return "kComposeNeverShowOnThisSiteAgain";
    case SuggestionType::kComposeProactiveNudge:
      return "kComposeProactiveNudge";
    case SuggestionType::kComposeSavedStateNotification:
      return "kComposeSavedStateNotification";
    case SuggestionType::kDatalistEntry:
      return "kDatalistEntry";
    case SuggestionType::kPasswordEntry:
      return "kPasswordEntry";
    case SuggestionType::kAllSavedPasswordsEntry:
      return "kAllSavedPasswordsEntry";
    case SuggestionType::kGeneratePasswordEntry:
      return "kGeneratePasswordEntry";
    case SuggestionType::kAccountStoragePasswordEntry:
      return "kAccountStoragePasswordEntry";
    case SuggestionType::kPasswordFieldByFieldFilling:
      return "kPasswordFieldByFieldFilling";
    case SuggestionType::kFillPassword:
      return "kFillPassword";
    case SuggestionType::kViewPasswordDetails:
      return "kViewPasswordDetails";
    case SuggestionType::kCreditCardEntry:
      return "kCreditCardEntry";
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
      return "kInsecureContextPaymentDisabledMessage";
    case SuggestionType::kSaveAndFillCreditCardEntry:
      return "KSaveAndFillCreditCardEntry";
    case SuggestionType::kScanCreditCard:
      return "kScanCreditCard";
    case SuggestionType::kVirtualCreditCardEntry:
      return "kVirtualCreditCardEntry";
    case SuggestionType::kIbanEntry:
      return "kIbanEntry";
    case SuggestionType::kBnplEntry:
      return "kBnplEntry";
    case SuggestionType::kCreateNewPlusAddress:
      return "kCreateNewPlusAddress";
    case SuggestionType::kCreateNewPlusAddressInline:
      return "kCreateNeWPlusAddressInline";
    case SuggestionType::kFillExistingPlusAddress:
      return "kFillExistingPlusAddress";
    case SuggestionType::kPlusAddressError:
      return "kPlusAddressError";
    case SuggestionType::kMerchantPromoCodeEntry:
      return "kMerchantPromoCodeEntry";
    case SuggestionType::kSeePromoCodeDetails:
      return "kSeePromoCodeDetails";
    case SuggestionType::kWebauthnCredential:
      return "kWebauthnCredential";
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
      return "kWebauthnSignInWithAnotherDevice";
    case SuggestionType::kIdentityCredential:
      return "kIdentityCredential";
    case SuggestionType::kTitle:
      return "kTitle";
    case SuggestionType::kSeparator:
      return "kSeparator";
    case SuggestionType::kUndoOrClear:
      return "kUndoOrClear";
    case SuggestionType::kMixedFormMessage:
      return "kMixedFormMessage";
    case SuggestionType::kDevtoolsTestAddresses:
      return "kDevtoolsTestAddresses";
    case SuggestionType::kDevtoolsTestAddressByCountry:
      return "kDevtoolsTestAddressByCountry";
    case SuggestionType::kDevtoolsTestAddressEntry:
      return "kDevtoolsTestAddressEntry";
    case SuggestionType::kFillAutofillAi:
      return "kFillAutofillAi";
    case SuggestionType::kPendingStateSignin:
      return "kPendingStateSignin";
    case SuggestionType::kLoyaltyCardEntry:
      return "kLoyaltyCardEntry";
    case SuggestionType::kHomeAndWorkAddressEntry:
      return "kHomeAndWorkAddressEntry";
  }
  NOTREACHED();
}

std::string SuggestionTypeToString(SuggestionType type) {
  return std::string(SuggestionTypeToStringView(type));
}

std::ostream& operator<<(std::ostream& os, SuggestionType type) {
  os << SuggestionTypeToStringView(type);
  return os;
}

}  // namespace autofill
