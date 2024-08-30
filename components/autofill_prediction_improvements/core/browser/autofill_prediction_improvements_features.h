// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FEATURES_H_
#define COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FEATURES_H_

#include "base/feature_list.h"

namespace autofill_prediction_improvements {

BASE_DECLARE_FEATURE(kAutofillPredictionImprovements);

bool IsAutofillPredictionImprovementsEnabled();

// Returns `true` if field trial parameter "skip_allowlist" for feature
// `kAutofillPredictionImprovements` is set to "true". If `true`, optimization
// guide `AUTOFILL_PREDICTION_IMPROVEMENTS_ALLOWLIST` will not be checked for
// possible application on the main frame's last committed URL.
bool ShouldSkipAllowlist();

}  // namespace autofill_prediction_improvements

#endif  // COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FEATURES_H_
