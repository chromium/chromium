// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/filling_product.h"

#include "base/notreached.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#include "components/autofill/android/main_autofill_jni_headers/FillingProductBridge_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

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
    case FillingProduct::kPasskey:
      return "Passkey";
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
    case FillingProduct::kDataList:
      return "DataList";
    case FillingProduct::kOneTimePassword:
      return "OneTimePassword";
  }
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
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kPendingStateSignin:
      return FillingProduct::kPassword;
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
      return FillingProduct::kCompose;
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kManagePlusAddress:
      return FillingProduct::kPlusAddresses;
    case SuggestionType::kDatalistEntry:
      return FillingProduct::kDataList;
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
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kLoyaltyCardEntry:
    case SuggestionType::kManageLoyaltyCard:
      return FillingProduct::kLoyaltyCard;
    case SuggestionType::kIdentityCredential:
      return FillingProduct::kIdentityCredential;
    case SuggestionType::kOneTimePasswordEntry:
      return FillingProduct::kOneTimePassword;
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
      return FillingProduct::kPasskey;
  }
  NOTREACHED();
}

#if BUILDFLAG(IS_ANDROID)
static jint JNI_FillingProductBridge_GetFillingProductFromSuggestionType(
    JNIEnv* env,
    jint type) {
  SuggestionType suggestion_type = static_cast<SuggestionType>(type);
  return static_cast<jint>(
      GetFillingProductFromSuggestionType(suggestion_type));
}
#endif  // BUILDFLAG(IS_ANDROID)

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
    case kOneTimePassword:
      return FillingProduct::kOneTimePassword;
  }
  NOTREACHED();
}

}  // namespace autofill

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(FillingProductBridge)
#endif
