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

// TODO(crbug.com/395555410): Remove.
BASE_DECLARE_FEATURE(kAutofillAi);

inline constexpr base::FeatureParam<base::TimeDelta> kExecutionTimeout{
    &kAutofillAi, /*name=*/"execution_timeout",
    /*default_value=*/base::Seconds(10)};

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
