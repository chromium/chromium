// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_types.h"
#include "base/containers/contains.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_util.h"

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
    case FieldTypeGroup::kIban:
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

bool FieldHasExpirationDateType(const AutofillField* field) {
  const std::vector<ServerFieldType> kExpirationDateTypes = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR,
      CREDIT_CARD_EXP_4_DIGIT_YEAR, CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
      CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR};
  return base::Contains(kExpirationDateTypes, field->Type().GetStorableType());
}

bool FormHasAllEmtyCreditCardFields(const FormStructure& form_structure) {
  bool has_card_number_field = base::ranges::any_of(
      form_structure, [](const std::unique_ptr<AutofillField>& autofill_field) {
        return autofill_field->Type().GetStorableType() ==
                   ServerFieldType::CREDIT_CARD_NUMBER &&
               SanitizedFieldIsEmpty(autofill_field->value);
      });

  bool has_expiration_date_field = base::ranges::any_of(
      form_structure, [](const std::unique_ptr<AutofillField>& autofill_field) {
        return FieldHasExpirationDateType(autofill_field.get()) &&
               SanitizedFieldIsEmpty(autofill_field->value);
      });

  return has_card_number_field && has_expiration_date_field;
}

}  // namespace autofill
