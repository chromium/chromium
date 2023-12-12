// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling_product.h"

#include "base/notreached.h"

namespace autofill {

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
    case PopupItemId::kAddressEntryNotSelectable:
      return FillingProduct::kAddress;
    case PopupItemId::kCreditCardEntry:
    case PopupItemId::kScanCreditCard:
    case PopupItemId::kVirtualCreditCardEntry:
    case PopupItemId::kCreditCardFieldByFieldFilling:
    case PopupItemId::kPaymentsEntryNotSelectable:
      return FillingProduct::kCreditCard;
    case PopupItemId::kMerchantPromoCodeEntry:
    case PopupItemId::kSeePromoCodeDetails:
      return FillingProduct::kMerchantPromoCode;
    case PopupItemId::kIbanEntry:
      return FillingProduct::kIban;
    case PopupItemId::kAutocompleteEntry:
    case PopupItemId::kDatalistEntry:
      return FillingProduct::kAutocomplete;
    case PopupItemId::kPasswordEntry:
    case PopupItemId::kUsernameEntry:
    case PopupItemId::kAllSavedPasswordsEntry:
    case PopupItemId::kGeneratePasswordEntry:
    case PopupItemId::kShowAccountCards:
    case PopupItemId::kPasswordAccountStorageOptIn:
    case PopupItemId::kPasswordAccountStorageOptInAndGenerate:
    case PopupItemId::kAccountStoragePasswordEntry:
    case PopupItemId::kAccountStorageUsernameEntry:
    case PopupItemId::kPasswordAccountStorageReSignin:
    case PopupItemId::kPasswordAccountStorageEmpty:
    case PopupItemId::kWebauthnCredential:
    case PopupItemId::kWebauthnSignInWithAnotherDevice:
      return FillingProduct::kPasswordManager;
    case PopupItemId::kCompose:
      return FillingProduct::kCompose;
    case PopupItemId::kCreateNewPlusAddress:
    case PopupItemId::kFillExistingPlusAddress:
      return FillingProduct::kPlusAddresses;
    case PopupItemId::kAutofillOptions:
    case PopupItemId::kSeparator:
    case PopupItemId::kClearForm:
    case PopupItemId::kMixedFormMessage:
    case PopupItemId::kInsecureContextPaymentDisabledMessage:
      return FillingProduct::kNone;
  }
}

}  // namespace autofill
