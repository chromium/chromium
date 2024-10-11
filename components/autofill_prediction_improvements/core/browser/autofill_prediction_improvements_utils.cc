// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_features.h"

namespace autofill_prediction_improvements {

using autofill::AutofillField;
using autofill::FieldTypeGroup;

namespace {

// Indicates if a `field` that has an autofill type that corresponds to a
// specific group is excluded from being supported by improved predictions.
bool FieldHasExclusiveAutofillType(const AutofillField& field) {
  switch (field.Type().group()) {
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
}  // namespace

bool IsFieldEligibleByTypeCriteria(const autofill::AutofillField& field) {
  // If a field type's group corresponds to a payment method or credentials, it
  // is not eligible even if it has the corresponding parsed field type.
  if (FieldHasExclusiveAutofillType(field)) {
    return false;
  }

#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  if (field.heuristic_type(
          autofill::HeuristicSource::kPredictionImprovementRegexes) ==
      autofill::IMPROVED_PREDICTION) {
    return true;
  }
#else
  if (field.Type().GetStorableType() == autofill::IMPROVED_PREDICTION) {
    return true;
  }
#endif
  else if (autofill::IsAddressType(field.Type().GetStorableType())) {
    return true;
  }

  return false;
}

bool IsFormEligibleForFillingByFieldCriteria(
    const autofill::FormStructure& form) {
  int total_number_of_fillable_fields = 0;

  // For a field to be fillable
  //  - it must have the correct field type.
  //  - its value must be empty.
  //  - it has to be focusable. In field filling skip reasons select fields may
  //    be unfocusable, for the estimated `total_number_of_fillable_fields`
  //    however that exception has shown to offer suggestions too often.
  for (const std::unique_ptr<AutofillField>& form_field : form.fields()) {
    if (IsFieldEligibleByTypeCriteria(*form_field) &&
        form_field->value(autofill::ValueSemantics::kCurrent).empty() &&
        form_field->IsFocusable()) {
      ++total_number_of_fillable_fields;
    }
  }

  return total_number_of_fillable_fields >=
         kMinimumNumberOfEligibleFieldsForFilling.Get();
}

bool IsFormEligibleForImportByFieldCriteria(
    const autofill::FormStructure& form) {
  int total_number_of_importable_fields = 0;

  // For a field to be importable it must have the correct field type and the
  // value must not have been marked as potentially sensitive.
  for (const std::unique_ptr<AutofillField>& form_field : form.fields()) {
    if (IsFieldEligibleByTypeCriteria(*form_field) &&
        !form_field->value_identified_as_potentially_sensitive()) {
      ++total_number_of_importable_fields;
    }
  }

  return total_number_of_importable_fields >=
         kMinimumNumberOfEligibleFieldsForImport.Get();
}

}  // namespace autofill_prediction_improvements
