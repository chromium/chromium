// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/custom_form_controls/metrics/custom_form_control_metrics.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace autofill::autofill_metrics {

void LogCustomFormControlFillLatency(base::TimeDelta latency) {
  base::UmaHistogramTimes("Autofill.CustomFormControls.FillLatency", latency);
}

base::OnceCallback<void(bool success)>
CreateCustomFormControlScriptLoadLoggingCallback() {
  return base::BindOnce(
      [](base::TimeTicks load_start_time, bool success) {
        if (success) {
          base::UmaHistogramTimes(
              "Autofill.CustomFormControls.ScriptLoadLatency",
              base::TimeTicks::Now() - load_start_time);
        }
      },
      base::TimeTicks::Now());
}

}  // namespace autofill::autofill_metrics
