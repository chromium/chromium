// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PROFILE_TOKEN_QUALITY_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PROFILE_TOKEN_QUALITY_METRICS_H_

#include <vector>

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill::autofill_metrics {

// Emits metrics for the `ProfileTokenQuality` of every provided `profiles`.
// See implementation for an overview of all the metrics.
void LogStoredProfileTokenQualityMetrics(
    const std::vector<AutofillProfile*>& profiles);

// Considers all autofilled fields of the `form`. For each of them, it emits
// the total number of observations that the profile used for filling had for
// the field's type.
// Additionally, for every profile that was used to fill at least one field in
// the form, the total number of stored observations are emitted.
void LogObservationCountBeforeSubmissionMetric(const FormStructure& form,
                                               const PersonalDataManager& pdm);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PROFILE_TOKEN_QUALITY_METRICS_H_
