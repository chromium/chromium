// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FEATURES_H_
#define COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace autofill_prediction_improvements {

BASE_DECLARE_FEATURE(kAutofillPredictionImprovements);

inline constexpr base::FeatureParam<bool> kSkipAllowlist{
    &kAutofillPredictionImprovements, /*name=*/"skip_allowlist",
    /*default_value=*/false};

inline constexpr base::FeatureParam<int>
    kMinimumNumberOfEligibleFieldsForFilling{
        &kAutofillPredictionImprovements,
        /*name=*/"minimum_number_of_eligible_fields_for_filling",
        /*default_value=*/1};

inline constexpr base::FeatureParam<int>
    kMinimumNumberOfEligibleFieldsForImport{
        &kAutofillPredictionImprovements,
        /*name=*/"minimum_number_of_eligible_fields_for_import",
        /*default_value=*/1};

inline constexpr base::FeatureParam<bool> kTriggerAutomatically{
    &kAutofillPredictionImprovements, /*name=*/"trigger_automatically",
    /*default_value=*/false};

bool IsAutofillPredictionImprovementsEnabled();

}  // namespace autofill_prediction_improvements

#endif  // COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FEATURES_H_
