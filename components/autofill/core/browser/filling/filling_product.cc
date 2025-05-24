// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/filling_product.h"

#include "base/notreached.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

namespace autofill {

// LINT.IfChange(FillingProductToString)
std::string FillingProductToString(FillingProduct filling_product) {
  switch (filling_product) {
    case FillingProduct::kNone:
      return "None";
    case FillingProduct::kAddress:
      return "Address";
    case FillingProduct::kCreditCard:
      return "CreditCard";
    case FillingProduct::kMerchantPromoCode:
      return "MerchantPromoCode";
    case FillingProduct::kIban:
      return "Iban";
    case FillingProduct::kAutocomplete:
      return "Autocomplete";
    case FillingProduct::kPassword:
      return "Password";
    case FillingProduct::kCompose:
      return "Compose";
    case FillingProduct::kPlusAddresses:
      return "PlusAddresses";
    case FillingProduct::kAutofillAi:
      return "AutofillAi";
    case FillingProduct::kLoyaltyCard:
      return "LoyaltyCard";
    case FillingProduct::kIdentityCredential:
      return "IdentityCredential";
  };
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/histograms.xml:Autofill.FillingProduct)

FillingProduct GetFillingProductFromSuggestionType(SuggestionType type) {
  switch (type) {
    case SuggestionType::kAddressEntry:
    case SuggestionType::kAddressEntryOnTyping:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kManageAddress:
    case SuggestionType::kHomeAndWorkAddressEntry:
      return FillingProduct::kAddress;
    case SuggestionType::kBnplEntry:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kVirtualCreditCardEntry:
      return FillingProduct::kCreditCard;
    case SuggestionType::kMerchantPromoCodeEntry:
      return FillingProduct::kMerchantPromoCode;
    case SuggestionType::kIbanEntry:
    case SuggestionType::kManageIban:
      return FillingProduct::kIban;
    case SuggestionType::kAutocompleteEntry:
      return FillingProduct::kAutocomplete;
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kFillPassword:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kPendingStateSignin:
      return FillingProduct::kPassword;
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
      return FillingProduct::kCompose;
    case SuggestionType::kCreateNewPlusAddress:
    case SuggestionType::kCreateNewPlusAddressInline:
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kPlusAddressError:
      return FillingProduct::kPlusAddresses;
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kSeparator:
    case SuggestionType::kTitle:
    case SuggestionType::kUndoOrClear:
      return FillingProduct::kNone;
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kManageAutofillAi:
      return FillingProduct::kAutofillAi;
    case SuggestionType::kLoyaltyCardEntry:
    case SuggestionType::kManageLoyaltyCard:
      return FillingProduct::kLoyaltyCard;
    case SuggestionType::kIdentityCredential:
      return FillingProduct::kIdentityCredential;
  }
  NOTREACHED();
}

FillingProduct GetFillingProductFromFieldTypeGroup(
    FieldTypeGroup field_type_group) {
  using enum FieldTypeGroup;
  switch (field_type_group) {
    case kUnfillable:
    case kTransaction:
    case kNoGroup:
      return FillingProduct::kNone;
    case kName:
    case kEmail:
    case kCompany:
    case kAddress:
    case kPhone:
      return FillingProduct::kAddress;
    case kCreditCard:
    case kStandaloneCvcField:
      return FillingProduct::kCreditCard;
    case kPasswordField:
    case kUsernameField:
      return FillingProduct::kPassword;
    case kIban:
      return FillingProduct::kIban;
    case kAutofillAi:
      return FillingProduct::kAutofillAi;
    case kLoyaltyCard:
      return FillingProduct::kLoyaltyCard;
  }
  NOTREACHED();
}

FillingProduct GetPreferredSuggestionFillingProduct(
    FieldType trigger_field_type,
    AutofillSuggestionTriggerSource suggestion_trigger_source) {
  FillingProduct filling_product = GetFillingProductFromFieldTypeGroup(
      GroupTypeOfFieldType(trigger_field_type));
  // Autofill suggestions fallbacks to autocomplete if no product could be
  // inferred from the suggestion context.
  return filling_product == FillingProduct::kNone
             ? FillingProduct::kAutocomplete
             : filling_product;
}

}  // namespace autofill
