// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_QUALITY_METRICS_FILLING_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_QUALITY_METRICS_FILLING_H_

#include "components/autofill/core/browser/form_structure.h"

namespace autofill::autofill_metrics {

// Enum defining buckets "autofilled" and "not autofilled" for the
// Autofill.DataUtilization.* UMA metric.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutofillDataUtilization {
  // On form submission, `FormFieldData::is_autofilled_` is `false`.
  kNotAutofilled = 0,
  // On form submission, `FormFieldData::is_autofilled_` is `true`.
  kAutofilled = 1,
  kMaxValue = kAutofilled
};

// Logs filling quality metrics.
void LogFillingQualityMetrics(const FormStructure& form);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_QUALITY_METRICS_FILLING_H_
