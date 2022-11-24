// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

FormType FieldTypeGroupToFormType(FieldTypeGroup field_type_group) {
  switch (field_type_group) {
    case FieldTypeGroup::kName:
    case FieldTypeGroup::kNameBilling:
    case FieldTypeGroup::kEmail:
    case FieldTypeGroup::kCompany:
    case FieldTypeGroup::kAddressHome:
    case FieldTypeGroup::kAddressBilling:
    case FieldTypeGroup::kPhoneHome:
    case FieldTypeGroup::kPhoneBilling:
    case FieldTypeGroup::kBirthdateField:
      return FormType::kAddressForm;
    case FieldTypeGroup::kCreditCard:
      return FormType::kCreditCardForm;
    case FieldTypeGroup::kUsernameField:
    case FieldTypeGroup::kPasswordField:
      return FormType::kPasswordForm;
    case FieldTypeGroup::kNoGroup:
    case FieldTypeGroup::kTransaction:
    case FieldTypeGroup::kUnfillable:
      return FormType::kUnknownFormType;
  }
}

base::StringPiece FormTypeToStringPiece(FormType form_type) {
  switch (form_type) {
    case FormType::kAddressForm:
      return "Address";
    case FormType::kCreditCardForm:
      return "CreditCard";
    case FormType::kPasswordForm:
      return "Password";
    case FormType::kUnknownFormType:
      return "Unknown";
  }

  NOTREACHED();
  return "UnknownFormType";
}

}  // namespace autofill
