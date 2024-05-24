// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_QUALITY_METRICS_FILLING_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_QUALITY_METRICS_FILLING_H_

#include "components/autofill/core/browser/form_structure.h"

namespace autofill::autofill_metrics {

// Logs filling quality metrics.
void LogFillingQualityMetrics(const FormStructure& form);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_QUALITY_METRICS_FILLING_H_
