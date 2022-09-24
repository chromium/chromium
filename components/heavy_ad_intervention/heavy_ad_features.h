// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAVY_AD_INTERVENTION_HEAVY_AD_FEATURES_H_
#define COMPONENTS_HEAVY_AD_INTERVENTION_HEAVY_AD_FEATURES_H_

#include "base/feature_list.h"

namespace heavy_ad_intervention {

namespace features {

// Param that enabled heavy ad intervention with reporting only, does not
// unloaded the ads.
extern const char kHeavyAdReportingOnlyParamName[];

// Param that enabled sending intervention reports for frames unloaded by heavy
// ad intervention.
extern const char kHeavyAdReportingEnabledParamName[];

BASE_DECLARE_FEATURE(kHeavyAdIntervention);
BASE_DECLARE_FEATURE(kHeavyAdInterventionWarning);
BASE_DECLARE_FEATURE(kHeavyAdPrivacyMitigations);

}  // namespace features

}  // namespace heavy_ad_intervention

#endif  // COMPONENTS_HEAVY_AD_INTERVENTION_HEAVY_AD_FEATURES_H_
