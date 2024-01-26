// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling_product.h"

#include "base/notreached.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

namespace autofill {

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
  };
  NOTREACHED_NORETURN();
}

FillingProduct GetFillingProductFromPopupItemId(PopupItemId popup_item_id) {
  switch (popup_item_id) {
    case PopupItemId::kAddressEntry:
    case PopupItemId::kFillFullAddress:
    case PopupItemId::kFillFullName:
    case PopupItemId::kFillEverythingFromAddressProfile:
    case PopupItemId::kFillFullPhoneNumber:
    case PopupItemId::kFillFullEmail:
    case PopupItemId::kAddressFieldByFieldFilling:
    case PopupItemId::kEditAddressProfile:
    case PopupItemId::kDeleteAddressProfile:
    case PopupItemId::kDevtoolsTestAddresses:
    case PopupItemId::kDevtoolsTestAddressEntry:
      return FillingProduct::kAddress;
    case PopupItemId::kCreditCardEntry:
    case PopupItemId::kCreditCardFieldByFieldFilling:
    case PopupItemId::kVirtualCreditCardEntry:
    case PopupItemId::kScanCreditCard:
    case PopupItemId::kShowAccountCards:
      return FillingProduct::kCreditCard;
    case PopupItemId::kMerchantPromoCodeEntry:
      return FillingProduct::kMerchantPromoCode;
    case PopupItemId::kIbanEntry:
      return FillingProduct::kIban;
    case PopupItemId::kAutocompleteEntry:
      return FillingProduct::kAutocomplete;
    case PopupItemId::kPasswordEntry:
    case PopupItemId::kUsernameEntry:
    case PopupItemId::kAllSavedPasswordsEntry:
    case PopupItemId::kGeneratePasswordEntry:
    case PopupItemId::kPasswordAccountStorageOptIn:
    case PopupItemId::kPasswordAccountStorageOptInAndGenerate:
    case PopupItemId::kAccountStoragePasswordEntry:
    case PopupItemId::kAccountStorageUsernameEntry:
    case PopupItemId::kPasswordAccountStorageReSignin:
    case PopupItemId::kPasswordAccountStorageEmpty:
    case PopupItemId::kWebauthnCredential:
    case PopupItemId::kWebauthnSignInWithAnotherDevice:
      return FillingProduct::kPassword;
    case PopupItemId::kCompose:
      return FillingProduct::kCompose;
    case PopupItemId::kCreateNewPlusAddress:
    case PopupItemId::kFillExistingPlusAddress:
      return FillingProduct::kPlusAddresses;
    case PopupItemId::kAutofillOptions:
    case PopupItemId::kSeePromoCodeDetails:
    case PopupItemId::kSeparator:
    case PopupItemId::kClearForm:
    case PopupItemId::kDatalistEntry:
    case PopupItemId::kMixedFormMessage:
    case PopupItemId::kInsecureContextPaymentDisabledMessage:
      return FillingProduct::kNone;
  }
}

FillingProduct GetFillingProductFromFieldTypeGroup(
    FieldTypeGroup field_type_group) {
  switch (field_type_group) {
    case FieldTypeGroup::kUnfillable:
    case FieldTypeGroup::kTransaction:
    case FieldTypeGroup::kNoGroup:
      return FillingProduct::kNone;
    case FieldTypeGroup::kName:
    case FieldTypeGroup::kEmail:
    case FieldTypeGroup::kCompany:
    case FieldTypeGroup::kAddress:
    case FieldTypeGroup::kPhone:
    case FieldTypeGroup::kBirthdateField:
      return FillingProduct::kAddress;
    case FieldTypeGroup::kCreditCard:
      return FillingProduct::kCreditCard;
    case FieldTypeGroup::kPasswordField:
    case FieldTypeGroup::kUsernameField:
      return FillingProduct::kPassword;
    case FieldTypeGroup::kIban:
      return FillingProduct::kIban;
  }
  NOTREACHED_NORETURN();
}

FillingProduct GetPreferredSuggestionFillingProduct(
    FieldType trigger_field_type,
    AutofillSuggestionTriggerSource suggestion_trigger_source) {
  if (suggestion_trigger_source ==
      mojom::AutofillSuggestionTriggerSource::kManualFallbackAddress) {
    return FillingProduct::kAddress;
  }
  if (suggestion_trigger_source ==
      mojom::AutofillSuggestionTriggerSource::kManualFallbackPayments) {
    return FillingProduct::kCreditCard;
  }
  FillingProduct filling_product = GetFillingProductFromFieldTypeGroup(
      GroupTypeOfFieldType(trigger_field_type));
  // Autofill suggestions fallbacks to autocomplete if no product could be
  // inferred from the suggestion context.
  return filling_product == FillingProduct::kNone
             ? FillingProduct::kAutocomplete
             : filling_product;
}

}  // namespace autofill
