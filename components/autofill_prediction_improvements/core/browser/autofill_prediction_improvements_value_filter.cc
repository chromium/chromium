// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_value_filter.h"

#include "components/autofill/core/browser/form_structure.h"

namespace autofill_prediction_improvements {

namespace {

using autofill::AutofillType;
using autofill::FieldTypeGroup;
using autofill::FieldTypeSet;
using autofill::FormStructure;

constexpr FieldTypeSet sensitive_types{
    autofill::PASSWORD, autofill::CREDIT_CARD_NUMBER, autofill::IBAN_VALUE,
    autofill::CREDIT_CARD_VERIFICATION_CODE,
    autofill::CREDIT_CARD_STANDALONE_VERIFICATION_CODE};

bool HasTypeSensitiveGroup(AutofillType type) {
  switch (type.group()) {
    case FieldTypeGroup::kNoGroup:
    case FieldTypeGroup::kName:
    case FieldTypeGroup::kEmail:
    case FieldTypeGroup::kCompany:
    case FieldTypeGroup::kAddress:
    case FieldTypeGroup::kPhone:
    case FieldTypeGroup::kUnfillable:
    case FieldTypeGroup::kPredictionImprovements:
      return false;
    case FieldTypeGroup::kCreditCard:
    case FieldTypeGroup::kPasswordField:
    case FieldTypeGroup::kTransaction:
    case FieldTypeGroup::kUsernameField:
    case FieldTypeGroup::kIban:
    case FieldTypeGroup::kStandaloneCvcField:
      return true;
  }
}

void ApplyFilter(autofill::AutofillField& field) {
  field.set_value_identified_as_potentially_sensitive(true);
}

// Filter all values that are contained in fields with a type from a sensitive
// form group like credentials and payment information.
int FilterSensitiveValuesByFieldType(FormStructure& form) {
  int removed_values = 0;
  for (auto& field : form) {
    if (HasTypeSensitiveGroup(field->Type())) {
      ApplyFilter(*field);
      ++removed_values;
    }
  }
  return removed_values;
}

// Filter sensitive values that have been filled with Autofill into arbitrary
// fields.
int FilterSensitiveValuesByFillingType(FormStructure& form) {
  int removed_values = 0;
  for (auto& field : form) {
    if (HasTypeSensitiveGroup(AutofillType(
            field->autofilled_type().value_or(autofill::UNKNOWN_TYPE)))) {
      ApplyFilter(*field);
      ++removed_values;
    }
  }
  return removed_values;
}

// Filter values that have been manually added by the user but resemble known
// sensitive values. A sensitive value can be a password or a credit card
// number, while usernames and dates are excluded due to the risk of false
// positives.
int FilterSensitiveValuesByPossibleFieldType(FormStructure& form) {
  int removed_values = 0;
  for (auto& field : form) {
    if (field->possible_types().contains_any(sensitive_types)) {
      ApplyFilter(*field);
      continue;
    }
    ++removed_values;
  }
  return removed_values;
}

// Filters values that are contained in password-type fields.
int FilterSensitiveValuesByInputType(FormStructure& form) {
  int removed_values = 0;
  for (auto& field : form) {
    if (field->form_control_type() ==
        autofill::FormControlType::kInputPassword) {
      ApplyFilter(*field);
      continue;
    }
    ++removed_values;
  }
  return removed_values;
}

}  // namespace

int FilterSensitiveValues(FormStructure& form) {
  int total_values_removed = 0;

  // For metrics purposes we will do the removals in sequence.
  // To be able to evaluate the single stages in a form-holistic manner, we keep
  // the loops separated.
  total_values_removed += FilterSensitiveValuesByFieldType(form);
  total_values_removed += FilterSensitiveValuesByFillingType(form);
  total_values_removed += FilterSensitiveValuesByPossibleFieldType(form);
  total_values_removed += FilterSensitiveValuesByInputType(form);

  return total_values_removed;
}

}  // namespace autofill_prediction_improvements
