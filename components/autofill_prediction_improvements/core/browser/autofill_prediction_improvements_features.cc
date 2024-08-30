// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_features.h"

#include "base/metrics/field_trial_params.h"

namespace autofill_prediction_improvements {

// Autofill offers improvements on how field types and filling values are
// predicted.
BASE_FEATURE(kAutofillPredictionImprovements,
             "AutofillPredictionImprovements",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAutofillPredictionImprovementsEnabled() {
  return base::FeatureList::IsEnabled(kAutofillPredictionImprovements);
}

bool ShouldSkipAllowlist() {
  // TODO(crbug.com/362659272): Change default value to `false`.
  return base::GetFieldTrialParamByFeatureAsBool(
      kAutofillPredictionImprovements, "skip_allowlist", true);
}

}  // namespace autofill_prediction_improvements
