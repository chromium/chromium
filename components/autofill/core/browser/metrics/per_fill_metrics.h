// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PER_FILL_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PER_FILL_METRICS_H_

#include "components/autofill/core/browser/filling/form_filler.h"

namespace autofill::autofill_metrics {

// Logs Autofill.NumberOfFieldsPerAutofill*.
//
// The `modified_fields_count` is the number of fields for which the browser
// process sends fill values to the renderer process (i.e., it takes into
// account the same-origin policy in case of a form with fields from different
// origins).
void LogNumberOfFieldsModifiedByAutofill(size_t modified_fields_count,
                                         const FillingPayload& filling_payload);

void LogRefillTriggerReason(RefillTriggerReason refill_trigger_reason);

void LogNumberOfFieldsModifiedByRefill(
    RefillTriggerReason refill_trigger_reason,
    size_t num_modified_fields);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PER_FILL_METRICS_H_
