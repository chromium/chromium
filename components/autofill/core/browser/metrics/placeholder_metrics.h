// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PLACEHOLDER_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PLACEHOLDER_METRICS_H_

#include <string>
#include <string_view>

#include "components/autofill/core/browser/autofill_field.h"

namespace autofill::autofill_metrics {

// Enum for logging if a field is pre-filled or empty on page load.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutofillPreFilledFieldStatus {
  // The field had a pre-filled value on page load.
  kPreFilledOnPageLoad = 0,
  // The field was empty on page load.
  kEmptyOnPageLoad = 1,
  kMaxValue = kEmptyOnPageLoad
};

// Enum for logging if the value of a pre-filled field was changed until form
// submission.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutofillPreFilledValueStatus {
  // The pre-filled field has kept its value at form submission. It was never
  // autofilled.
  kPreFilledValueNotChanged = 0,
  // At form submission, the pre-filled field has the same value as on page
  // load. It was autofilled at some point but also edited afterwards.
  kPreFilledValueWasManuallyRestoredAfterAutofill = 1,
  // At form submission, the pre-filled field has the same value as on page
  // load due to Autofill.
  kPreFilledValueWasRestoredByAutofill = 2,
  // The pre-filled field's value was changed to empty.
  kPreFilledValueChangedToEmpty = 3,
  // The pre-filled field was skipped during filling the form. The user then
  // changed the value to what Autofill would have filled.
  kPreFilledValueChangedToWhatWouldHaveBeenFilled = 4,
  // The pre-filled field's value was changed to another value matching its
  // field type.
  kPreFilledValueChangedToCorrespondingFieldType = 5,
  // The pre-filled field's value was changed to any other value.
  kPreFilledValueChanged = 6,
  kMaxValue = kPreFilledValueChanged
};

// Enum for logging if a pre-filled field had a placeholder classification.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutofillPreFilledFieldClassifications {
  // The field had a placeholder classification.
  kClassified = 0,
  // The field didn't have a placeholder classification.
  kNotClassified = 1,
  kMaxValue = kNotClassified
};

// Enum for logging the quality of a placeholder classification.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutofillPreFilledFieldClassificationsQuality {
  // The field was classified as a placeholder but its pre-filled value got
  // submitted.
  kPlaceholderValueNotChanged = 0,
  // The field was classified as a placeholder and its value was changed before
  // form submission.
  kPlaceholderValueChanged = 1,
  // The field was classified as meaningfully pre-filled and its pre-filled
  // value got submitted.
  kMeaningfullyPreFilledValueNotChanged = 2,
  // The field was classified as meaningfully pre-filled but its value was
  // changed before form submission.
  kMeaningfullyPreFilledValueChanged = 3,
  kMaxValue = kMeaningfullyPreFilledValueChanged
};

// Log if the field was pre-filled or empty on page load. Aggregated separately
// by `form_type_name` and `field_type`.
void LogPreFilledFieldStatus(std::string_view form_type_name,
                             std::optional<bool> initial_value_changed,
                             FieldType field_type);

// Log if the value of a pre-filled field was changed before form submission.
void LogPreFilledValueChanged(
    std::string_view form_type_name,
    std::optional<bool> initial_value_changed,
    const std::u16string& value,
    const std::vector<AutofillField::FieldLogEventType>& field_log_events,
    const FieldTypeSet& possible_types,
    FieldType field_type,
    bool is_autofilled);

// Logs if a pre-filled field had a placeholder classification. Also logs the
// quality of the classification if it exists.
void LogPreFilledFieldClassifications(
    std::string_view form_type_name,
    std::optional<bool> initial_value_changed,
    std::optional<bool> may_use_prefilled_placeholder);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PLACEHOLDER_METRICS_H_
