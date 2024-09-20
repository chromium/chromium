// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_features.h"


namespace autofill_prediction_improvements {

// Autofill offers improvements on how field types and filling values are
// predicted.
BASE_FEATURE(kAutofillPredictionImprovements,
             "AutofillPredictionImprovements",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAutofillPredictionImprovementsEnabled() {
  return base::FeatureList::IsEnabled(kAutofillPredictionImprovements);
}

}  // namespace autofill_prediction_improvements
