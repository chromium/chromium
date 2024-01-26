// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/placeholder_metrics.h"

#include "base/containers/adapters.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace autofill::autofill_metrics {

namespace {

// This function encodes the integer values of `field_type` and
// `prefilled_status` into a 16 bit integer. The lower four bits are used to
// encode the editing status and the higher 12 bits are used to encode the field
// type.
int GetFieldTypeAutofillPreFilledFieldsStatus(
    FieldType field_type,
    AutofillPreFilledFieldStatus prefilled_status) {
  static_assert(FieldType::MAX_VALID_FIELD_TYPE <= (UINT16_MAX >> 4),
                "Autofill::FieldType value needs more than 12 bits.");

  static_assert(static_cast<int>(AutofillPreFilledFieldStatus::kMaxValue) <=
                    (UINT16_MAX >> 12),
                "AutofillPreFilledFieldStatus value needs more than 4 bits");

  return (field_type << 4) | static_cast<int>(prefilled_status);
}

// Traverses `field_log_events` in reverse order to find a `FillFieldLogEvent`
// entry where its field
// `value_that_would_have_been_filled_in_a_prefilled_field_hash` has a value,
// then returns that field.
std::optional<size_t>
FindLatestValueThatWouldHaveBeenFilledInAPreFilledFieldHash(
    const std::vector<AutofillField::FieldLogEventType>& field_log_events) {
  for (const auto& log_event : base::Reversed(field_log_events)) {
    if (const FillFieldLogEvent* e =
            absl::get_if<FillFieldLogEvent>(&log_event)) {
      if (e->value_that_would_have_been_filled_in_a_prefilled_field_hash
              .has_value()) {
        return *e->value_that_would_have_been_filled_in_a_prefilled_field_hash;
      }
    }
  }
  return std::nullopt;
}

// Returns `true` if the field was skipped during filling because it was
// pre-filled and the user has edited the field such that, at form submission,
// it contained the value that would have been filled.  Note that this also
// returns `true` if Autofill was triggered from the field (e.g. a second time
// after the field wasn't filled the first time Autofill was triggered).
bool SkippedFieldValueChangedToWhatWouldHaveBeenFilled(
    const std::vector<AutofillField::FieldLogEventType>& field_log_events,
    const std::u16string& value) {
  DCHECK(!value.empty());
  std::optional<size_t> hash =
      FindLatestValueThatWouldHaveBeenFilledInAPreFilledFieldHash(
          field_log_events);
  return hash && base::FastHash(base::UTF16ToUTF8(value)) == *hash;
}

// Returns `true` if the field was autofilled before but isn't autofilled at
// form submission.
bool WasAutofilledAndThenEdited(
    const std::vector<AutofillField::FieldLogEventType>& field_log_events,
    const bool is_autofilled) {
  if (is_autofilled) {
    return false;
  }
  for (const AutofillField::FieldLogEventType& field_log_event :
       field_log_events) {
    if (const FillFieldLogEvent* e =
            absl::get_if<FillFieldLogEvent>(&field_log_event)) {
      if (e->autofill_skipped_status == FieldFillingSkipReason::kNotSkipped &&
          e->had_value_after_filling == OptionalBoolean::kTrue) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

void LogPreFilledFieldStatus(std::string_view form_type_name,
                             std::optional<bool> initial_value_changed,
                             autofill::FieldType field_type) {
  const AutofillPreFilledFieldStatus prefilled_status =
      initial_value_changed.has_value()
          ? AutofillPreFilledFieldStatus::kPreFilledOnPageLoad
          : AutofillPreFilledFieldStatus::kEmptyOnPageLoad;
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.PreFilledFieldStatus.", form_type_name}),
      prefilled_status);
  base::UmaHistogramSparse(
      "Autofill.PreFilledFieldStatus.ByFieldType",
      GetFieldTypeAutofillPreFilledFieldsStatus(field_type, prefilled_status));
}

void LogPreFilledValueChanged(
    std::string_view form_type_name,
    std::optional<bool> initial_value_changed,
    const std::u16string& value,
    const std::vector<AutofillField::FieldLogEventType>& field_log_events,
    const FieldTypeSet& possible_types,
    FieldType field_type,
    bool is_autofilled) {
  if (!initial_value_changed.has_value()) {
    return;
  }
  AutofillPreFilledValueStatus value_status =
      AutofillPreFilledValueStatus::kPreFilledValueChanged;
  if (!initial_value_changed.value()) {
    if (WasAutofilledAndThenEdited(field_log_events, is_autofilled)) {
      value_status = AutofillPreFilledValueStatus::
          kPreFilledValueWasManuallyRestoredAfterAutofill;
    } else if (is_autofilled) {
      value_status =
          AutofillPreFilledValueStatus::kPreFilledValueWasRestoredByAutofill;
    } else {
      value_status = AutofillPreFilledValueStatus::kPreFilledValueNotChanged;
    }
  } else if (value.empty()) {
    value_status = AutofillPreFilledValueStatus::kPreFilledValueChangedToEmpty;
  } else if (SkippedFieldValueChangedToWhatWouldHaveBeenFilled(field_log_events,
                                                               value)) {
    value_status = AutofillPreFilledValueStatus::
        kPreFilledValueChangedToWhatWouldHaveBeenFilled;
  } else if (possible_types.contains(field_type)) {
    value_status = AutofillPreFilledValueStatus::
        kPreFilledValueChangedToCorrespondingFieldType;
  }
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.PreFilledValueStatus.", form_type_name}),
      value_status);
}

void LogPreFilledFieldClassifications(
    std::string_view form_type_name,
    std::optional<bool> initial_value_changed,
    std::optional<bool> may_use_prefilled_placeholder) {
  if (!initial_value_changed.has_value()) {
    return;
  }
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.PreFilledFieldClassifications.", form_type_name}),
      may_use_prefilled_placeholder.has_value()
          ? AutofillPreFilledFieldClassifications::kClassified
          : AutofillPreFilledFieldClassifications::kNotClassified);

  if (may_use_prefilled_placeholder.has_value()) {
    const std::string name = base::StrCat(
        {"Autofill.PreFilledFieldClassificationsQuality.", form_type_name});
    AutofillPreFilledFieldClassificationsQuality sample =
        AutofillPreFilledFieldClassificationsQuality::
            kMeaningfullyPreFilledValueChanged;
    if (*initial_value_changed && *may_use_prefilled_placeholder) {
      sample = AutofillPreFilledFieldClassificationsQuality::
          kPlaceholderValueChanged;
    } else if (!*initial_value_changed && *may_use_prefilled_placeholder) {
      sample = AutofillPreFilledFieldClassificationsQuality::
          kPlaceholderValueNotChanged;
    } else if (!*initial_value_changed && !*may_use_prefilled_placeholder) {
      sample = AutofillPreFilledFieldClassificationsQuality::
          kMeaningfullyPreFilledValueNotChanged;
    }
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"Autofill.PreFilledFieldClassificationsQuality.", form_type_name}),
        sample);
  }
}

}  // namespace autofill::autofill_metrics
