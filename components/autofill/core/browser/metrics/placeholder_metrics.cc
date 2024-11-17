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

}  // namespace

void LogPreFilledFieldStatus(std::string_view form_type_name,
                             std::optional<bool> initial_value_changed,
                             FieldType field_type) {
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
