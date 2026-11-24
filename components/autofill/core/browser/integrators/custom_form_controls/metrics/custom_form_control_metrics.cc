// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/custom_form_controls/metrics/custom_form_control_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill::autofill_metrics {

void LogCustomFormControlFillLatency(base::TimeDelta latency) {
  base::UmaHistogramTimes("Autofill.CustomFormControls.FillLatency", latency);
}

}  // namespace autofill::autofill_metrics
