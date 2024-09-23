// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_QUALITY_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_QUALITY_METRICS_H_

#include "base/time/time.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/field_filling_stats_and_score_metrics.h"

namespace autofill::autofill_metrics {

// Logs quality metrics for the `form_structure`.
// This method should only be called after the possible field types have been
// set for each field.
// `interaction_time` corresponds to the user's first interaction with the form.
// `submission_time` corresponds to the form's submission time.
// `observed_submission` indicates whether this method is called as a result of
// observing a submission event (otherwise, it may be that an upload was
// triggered after a form was unfocused or a navigation occurred).
void LogQualityMetrics(
    const FormStructure& form_structure,
    base::TimeTicks load_time,
    base::TimeTicks interaction_time,
    base::TimeTicks submission_time,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    bool observed_submission);

// Log the quality of the heuristics and server predictions for the
// `form_structure` structure, if autocomplete attributes are present on the
// fields (they are used as golden truths).
void LogQualityMetricsBasedOnAutocomplete(
    const FormStructure& form_structure,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_QUALITY_METRICS_H_
