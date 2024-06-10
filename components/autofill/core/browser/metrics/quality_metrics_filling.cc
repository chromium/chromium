// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/quality_metrics_filling.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace autofill::autofill_metrics {

namespace {

// Heuristic used for filtering fields that are probably not fillable. The
// assumption is that autofilled values typically should have lengths well below
// 150 and that extremely long texts are outliers and should not influence the
// metrics a lot.
constexpr size_t kAutomationRateFieldSizeThreshold = 150;

// Records the percentage of input text field characters that were autofilled.
void LogAutomationRate(const FormStructure& form) {
  size_t total_length_autofilled_fields = 0;
  size_t total_length = 0;
  for (const auto& field : form.fields()) {
    if (!field->IsTextInputElement()) {
      continue;
    }
    // The field value at form submission should have changed since page load.
    if (!field->initial_value_changed().value_or(true)) {
      continue;
    }
    size_t field_size = field->value().size();
    // Skip fields containing too many characters to reduce distortion by
    // fields that are likely not autofillable.
    if (field_size > kAutomationRateFieldSizeThreshold) {
      continue;
    }
    if (field->is_autofilled()) {
      total_length_autofilled_fields += field_size;
    }
    total_length += field_size;
  }
  if (total_length > 0) {
    for (const auto form_type : form.GetFormTypes()) {
      // TODO(crrev.com/c/5499250): Use `FormTypeNameForLogging` once the CL is
      // merged.
      base::UmaHistogramPercentage(
          base::StrCat(
              {"Autofill.AutomationRate.", FormTypeToStringView(form_type)}),
          100 * total_length_autofilled_fields / total_length);
    }
  }
}

}  // namespace

void LogFillingQualityMetrics(const FormStructure& form) {
  LogAutomationRate(form);
}

}  // namespace autofill::autofill_metrics
