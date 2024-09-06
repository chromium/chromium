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
    case FieldTypeGroup::kEmail:
    case FieldTypeGroup::kCompany:
    case FieldTypeGroup::kAddress:
    case FieldTypeGroup::kPhone:
      return FormType::kAddressForm;
    case FieldTypeGroup::kCreditCard:
      return FormType::kCreditCardForm;
    case FieldTypeGroup::kUsernameField:
    case FieldTypeGroup::kPasswordField:
      return FormType::kPasswordForm;
    case FieldTypeGroup::kStandaloneCvcField:
      return FormType::kStandaloneCvcForm;
    case FieldTypeGroup::kIban:
    case FieldTypeGroup::kNoGroup:
    case FieldTypeGroup::kTransaction:
    case FieldTypeGroup::kUnfillable:
    case FieldTypeGroup::kPredictionImprovements:
      return FormType::kUnknownFormType;
  }
}

std::string_view FormTypeToStringView(FormType form_type) {
  switch (form_type) {
    case FormType::kAddressForm:
      return "Address";
    case FormType::kCreditCardForm:
      return "CreditCard";
    case FormType::kPasswordForm:
      return "Password";
    case FormType::kUnknownFormType:
      return "Unknown";
    case FormType::kStandaloneCvcForm:
      return "StandaloneCvc";
  }

  NOTREACHED();
}

// When adding a new return value, update variants "AutofillFormType.Fillable"
// and "AutofillFormType.Address" in
// tools/metrics/histograms/metadata/autofill/histograms.xml accordingly.
std::string_view FormTypeNameForLoggingToStringView(
    FormTypeNameForLogging form_type_name) {
  switch (form_type_name) {
    case FormTypeNameForLogging::kAddressForm:
      return "Address";
    case FormTypeNameForLogging::kCreditCardForm:
      return "CreditCard";
    case FormTypeNameForLogging::kPasswordForm:
      return "Password";
    case FormTypeNameForLogging::kUnknownFormType:
      return "Unknown";
    case FormTypeNameForLogging::kStandaloneCvcForm:
      return "StandaloneCvc";
    case FormTypeNameForLogging::kEmailOnlyForm:
      return "EmailOnly";
    case FormTypeNameForLogging::kPostalAddressForm:
      return "PostalAddress";
  }

  NOTREACHED();
}

bool FormHasAllCreditCardFields(const FormStructure& form_structure) {
  bool has_card_number_field = std::ranges::any_of(
      form_structure, [](const std::unique_ptr<AutofillField>& autofill_field) {
        return autofill_field->Type().GetStorableType() ==
               FieldType::CREDIT_CARD_NUMBER;
      });

  bool has_expiration_date_field = std::ranges::any_of(
      form_structure, [](const std::unique_ptr<AutofillField>& autofill_field) {
        return autofill_field->HasExpirationDateType();
      });

  return has_card_number_field && has_expiration_date_field;
}

}  // namespace autofill
