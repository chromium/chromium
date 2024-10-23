// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FEATURES_H_
#define COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

class PrefService;

namespace autofill_prediction_improvements {

BASE_DECLARE_FEATURE(kAutofillPredictionImprovements);

BASE_DECLARE_FEATURE(kAutofillPredictionBootstrapping);

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

inline constexpr base::FeatureParam<base::TimeDelta> kExecutionTimeout{
    &kAutofillPredictionImprovements, /*name=*/"execution_timeout",
    /*default_value=*/base::Seconds(10)};

inline constexpr base::FeatureParam<bool> kExtractAXTreeForPredictions{
    &kAutofillPredictionImprovements,
    /*name=*/"extract_ax_tree_for_predictions",
    /*default_value=*/false};

inline constexpr base::FeatureParam<bool> kShowDetailsText{
    &kAutofillPredictionImprovements,
    /*name=*/"show_details_text",
    /*default_value=*/false};

// Indicates whether Autofill Prediction Improvements are available (but not
// necessary enabled). This considers the AutofillPredictionSettings policy.
bool IsAutofillPredictionImprovementsSupported(const PrefService* prefs);

}  // namespace autofill_prediction_improvements

#endif  // COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FEATURES_H_
