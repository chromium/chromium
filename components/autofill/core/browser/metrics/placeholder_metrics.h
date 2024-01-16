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

// Logs if the field was pre-filled or empty on page load. Aggregated separately
// by `form_type_name` and `field_type`.
void LogPreFilledFieldStatus(std::string_view form_type_name,
                             std::optional<bool> initial_value_changed,
                             FieldType field_type);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PLACEHOLDER_METRICS_H_
