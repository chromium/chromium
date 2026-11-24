// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_METRICS_CUSTOM_FORM_CONTROL_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_METRICS_CUSTOM_FORM_CONTROL_METRICS_H_

#include "base/functional/callback_forward.h"
#include "base/time/time.h"

namespace autofill::autofill_metrics {

// Logs the time it took to fill the custom form controls of a form, from when
// the fill of the form started until the last custom form control was handled.
void LogCustomFormControlFillLatency(base::TimeDelta latency);

// Returns a callback that logs the time since this call if the custom form
// control script was loaded successfully.
base::OnceCallback<void(bool success)>
CreateCustomFormControlScriptLoadLoggingCallback();

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_METRICS_CUSTOM_FORM_CONTROL_METRICS_H_
