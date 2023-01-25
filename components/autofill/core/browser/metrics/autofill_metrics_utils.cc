// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"

#include "base/check.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"

namespace autofill {

using FieldFillingStatus = AutofillMetrics::FieldFillingStatus;

void FormGroupFillingStats::AddFieldFillingStatus(FieldFillingStatus status) {
  switch (status) {
    case FieldFillingStatus::kAccepted:
      num_accepted++;
      return;
    case FieldFillingStatus::kCorrectedToSameType:
      num_corrected_to_same_type++;
      return;
    case FieldFillingStatus::kCorrectedToDifferentType:
      num_corrected_to_different_type++;
      return;
    case FieldFillingStatus::kCorrectedToUnknownType:
      num_corrected_to_unknown_type++;
      return;
    case FieldFillingStatus::kCorrectedToEmpty:
      num_corrected_to_empty++;
      return;
    case FieldFillingStatus::kManuallyFilledToSameType:
      num_manually_filled_to_same_type++;
      return;
    case FieldFillingStatus::kManuallyFilledToDifferentType:
      num_manually_filled_to_differt_type++;
      return;
    case FieldFillingStatus::kManuallyFilledToUnknownType:
      num_manually_filled_to_unknown_type++;
      return;
    case FieldFillingStatus::kLeftEmpty:
      num_left_empty++;
      return;
  }
  NOTREACHED();
}

FieldFillingStatus GetFieldFillingStatus(const AutofillField& field) {
  const bool is_empty = field.IsEmpty();
  const bool possible_types_empty =
      !FieldHasMeaningfulPossibleFieldTypes(field);
  const bool possible_types_contain_type = TypeOfFieldIsPossibleType(field);

  if (field.is_autofilled)
    return FieldFillingStatus::kAccepted;

  if (field.previously_autofilled()) {
    if (is_empty)
      return FieldFillingStatus::kCorrectedToEmpty;

    if (possible_types_contain_type)
      return FieldFillingStatus::kCorrectedToSameType;

    if (possible_types_empty)
      return FieldFillingStatus::kCorrectedToUnknownType;

    return FieldFillingStatus::kCorrectedToDifferentType;
  }

  if (is_empty)
    return FieldFillingStatus::kLeftEmpty;

  if (possible_types_contain_type)
    return FieldFillingStatus::kManuallyFilledToSameType;

  if (possible_types_empty)
    return FieldFillingStatus::kManuallyFilledToUnknownType;

  return FieldFillingStatus::kManuallyFilledToDifferentType;
}

std::string GetMetricsSuffixByAutofillMethod(AutofillSuggestionMethod method) {
  switch (method) {
    case AutofillSuggestionMethod::KTouchToFillCreditCard:
      return "TouchToFill";
    case AutofillSuggestionMethod::kUnknown:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return "";
}

}  // namespace autofill
