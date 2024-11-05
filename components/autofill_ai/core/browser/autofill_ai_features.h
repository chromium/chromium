// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_FEATURES_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

class PrefService;

namespace autofill_ai {

BASE_DECLARE_FEATURE(kAutofillAi);

BASE_DECLARE_FEATURE(kAutofillAiBootstrapping);

inline constexpr base::FeatureParam<bool> kSkipAllowlist{
    &kAutofillAi, /*name=*/"skip_allowlist",
    /*default_value=*/false};

inline constexpr base::FeatureParam<int>
    kMinimumNumberOfEligibleFieldsForFilling{
        &kAutofillAi,
        /*name=*/"minimum_number_of_eligible_fields_for_filling",
        /*default_value=*/1};

inline constexpr base::FeatureParam<int>
    kMinimumNumberOfEligibleFieldsForImport{
        &kAutofillAi,
        /*name=*/"minimum_number_of_eligible_fields_for_import",
        /*default_value=*/1};

inline constexpr base::FeatureParam<bool> kTriggerAutomatically{
    &kAutofillAi, /*name=*/"trigger_automatically",
    /*default_value=*/false};

inline constexpr base::FeatureParam<base::TimeDelta> kExecutionTimeout{
    &kAutofillAi, /*name=*/"execution_timeout",
    /*default_value=*/base::Seconds(10)};

inline constexpr base::FeatureParam<bool> kExtractAXTreeForPredictions{
    &kAutofillAi,
    /*name=*/"extract_ax_tree_for_predictions",
    /*default_value=*/false};

inline constexpr base::FeatureParam<bool> kShowDetailsText{
    &kAutofillAi,
    /*name=*/"show_details_text",
    /*default_value=*/false};

// Feature param to send title and URL of the page the form is in. By default,
// the origin of the page is sent, with no title.
inline constexpr base::FeatureParam<bool> kSendTitleURL{
    &kAutofillAi,
    /*name=*/"send_title_url",
    /*default_value=*/false};

// Indicates whether Autofill Prediction Improvements are available (but not
// necessary enabled). This considers the AutofillPredictionSettings policy.
// If this function returns false, no AutofillAiClient
// should be instantiated.
bool IsAutofillAiSupported(const PrefService* prefs);

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_FEATURES_H_
