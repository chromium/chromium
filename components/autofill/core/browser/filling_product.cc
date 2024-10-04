// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling_product.h"

#include "base/notreached.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

namespace autofill {

// LINT.IfChange
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
    case FillingProduct::kStandaloneCvc:
      return "StandaloneCvc";
    case FillingProduct::kPredictionImprovements:
      return "PredictionImprovements";
  };
  NOTREACHED();
}
// LINT.ThenChange(
//   /tools/metrics/histograms/metadata/autofill/histograms.xml:Autofill.FillingProduct,
//   /tools/metrics/histograms/metadata/autofill/histograms.xml:Autofill.FillingProduct.Condensed
// )

FillingProduct GetFillingProductFromSuggestionType(SuggestionType type) {
  switch (type) {
    case SuggestionType::kAddressEntry:
    case SuggestionType::kFillFullAddress:
    case SuggestionType::kFillFullName:
    case SuggestionType::kFillEverythingFromAddressProfile:
    case SuggestionType::kFillFullPhoneNumber:
    case SuggestionType::kFillFullEmail:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kEditAddressProfile:
    case SuggestionType::kDeleteAddressProfile:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kManageAddress:
      return FillingProduct::kAddress;
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kCreditCardFieldByFieldFilling:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kShowAccountCards:
    case SuggestionType::kManageCreditCard:
      return FillingProduct::kCreditCard;
    case SuggestionType::kMerchantPromoCodeEntry:
      return FillingProduct::kMerchantPromoCode;
    case SuggestionType::kIbanEntry:
    case SuggestionType::kManageIban:
      return FillingProduct::kIban;
    case SuggestionType::kAutocompleteEntry:
      return FillingProduct::kAutocomplete;
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kPasswordAccountStorageOptIn:
    case SuggestionType::kPasswordAccountStorageOptInAndGenerate:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kPasswordAccountStorageReSignin:
    case SuggestionType::kPasswordAccountStorageEmpty:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
      return FillingProduct::kPassword;
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeSavedStateNotification:
      return FillingProduct::kCompose;
    case SuggestionType::kCreateNewPlusAddress:
    case SuggestionType::kCreateNewPlusAddressInline:
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kPlusAddressError:
      return FillingProduct::kPlusAddresses;
    case SuggestionType::kPredictionImprovementsFeedback:
      return FillingProduct::kPredictionImprovements;
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kUndoOrClear:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
      return FillingProduct::kNone;
    case SuggestionType::kRetrievePredictionImprovements:
    case SuggestionType::kPredictionImprovementsLoadingState:
    case SuggestionType::kFillPredictionImprovements:
    case SuggestionType::kPredictionImprovementsError:
    case SuggestionType::kEditPredictionImprovementsInformation:
      return FillingProduct::kPredictionImprovements;
  }
  NOTREACHED();
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
      return FillingProduct::kAddress;
    case FieldTypeGroup::kCreditCard:
      return FillingProduct::kCreditCard;
    case FieldTypeGroup::kStandaloneCvcField:
      return FillingProduct::kStandaloneCvc;
    case FieldTypeGroup::kPasswordField:
    case FieldTypeGroup::kUsernameField:
      return FillingProduct::kPassword;
    case FieldTypeGroup::kIban:
      return FillingProduct::kIban;
    case autofill::FieldTypeGroup::kPredictionImprovements:
      return FillingProduct::kPredictionImprovements;
  }
  NOTREACHED();
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
