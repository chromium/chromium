// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_value_filter.h"

#include "components/autofill/core/browser/form_structure.h"

namespace autofill_ai {

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
void FilterSensitiveValuesByFieldType(FormStructure& form) {
  for (auto& field : form) {
    if (HasTypeSensitiveGroup(field->Type())) {
      ApplyFilter(*field);
    }
  }
}

// Filter sensitive values that have been filled with Autofill into arbitrary
// fields.
void FilterSensitiveValuesByFillingType(FormStructure& form) {
  for (auto& field : form) {
    if (HasTypeSensitiveGroup(AutofillType(
            field->autofilled_type().value_or(autofill::UNKNOWN_TYPE)))) {
      ApplyFilter(*field);
    }
  }
}

// Filter values that have been manually added by the user but resemble known
// sensitive values. A sensitive value can be a password or a credit card
// number, while usernames and dates are excluded due to the risk of false
// positives.
void FilterSensitiveValuesByPossibleFieldType(FormStructure& form) {
  for (auto& field : form) {
    if (field->possible_types().contains_any(sensitive_types)) {
      auto x = field->possible_types();
      x.intersect(sensitive_types);
      ApplyFilter(*field);
      continue;
    }
  }
}

// Filters values that are contained in password-type fields.
void FilterSensitiveValuesByInputType(FormStructure& form) {
  for (auto& field : form) {
    if (field->form_control_type() ==
        autofill::FormControlType::kInputPassword) {
      ApplyFilter(*field);
      continue;
    }
  }
}

}  // namespace

void FilterSensitiveValues(FormStructure& form) {
  // For metrics purposes we will do the removals in sequence.
  // To be able to evaluate the single stages in a form-holistic manner, we keep
  // the loops separated.
  FilterSensitiveValuesByFieldType(form);
  FilterSensitiveValuesByFillingType(form);
  FilterSensitiveValuesByPossibleFieldType(form);
  FilterSensitiveValuesByInputType(form);
}

}  // namespace autofill_ai
