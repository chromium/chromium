// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/field_filling_skip_reason.h"

#include <string_view>

#include "base/notreached.h"

namespace autofill {

std::string_view GetSkipFieldFillLogMessage(
    FieldFillingSkipReason skip_reason) {
  switch (skip_reason) {
    case FieldFillingSkipReason::kNotInFilledSection:
      return "Skipped: Not part of filled section";
    case FieldFillingSkipReason::kNotFocused:
      return "Skipped: Only fill when focused";
    case FieldFillingSkipReason::kUnrecognizedAutocompleteAttribute:
      return "Skipped: Unrecognized autocomplete attribute";
    case FieldFillingSkipReason::kFormChanged:
      return "Skipped: Form has changed";
    case FieldFillingSkipReason::kInvisibleField:
      return "Skipped: Invisible field";
    case FieldFillingSkipReason::kValuePrefilled:
      return "Skipped: Value is prefilled";
    case FieldFillingSkipReason::kUserFilledFields:
      return "Skipped: User filled the field";
    case FieldFillingSkipReason::kAlreadyAutofilled:
      return "Skipped: Field is already autofilled.";
    case FieldFillingSkipReason::kNoFillableGroup:
      return "Skipped: Field type has no fillable group";
    case FieldFillingSkipReason::kRefillNotInInitialFill:
      return "Skipped: Refill field group different from initial filling group";
    case FieldFillingSkipReason::kExpiredCards:
      return "Skipped: Expired expiration date for credit card";
    case FieldFillingSkipReason::kFillingLimitReachedType:
      return "Skipped: Field type filling limit reached";
    case FieldFillingSkipReason::kFieldDoesNotMatchTargetFieldsSet:
      return "Skipped: The field type does not match the targeted fields.";
    case FieldFillingSkipReason::kFieldTypeUnrelated:
      return "Skipped: The field type is not related to the data used for "
             "filling.";
    case FieldFillingSkipReason::kNoValueToFill:
      return "Skipped: No value to fill.";
    case FieldFillingSkipReason::kAutofilledValueDidNotChange:
      return "Skipped: Field already autofilled with same value.";
    case FieldFillingSkipReason::kNotSkipped:
      return "Fillable";
    case FieldFillingSkipReason::kUnknown:
      NOTREACHED();
  }
}

}  // namespace autofill
