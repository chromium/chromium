// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heavy_ad_intervention/heavy_ad_features.h"

namespace heavy_ad_intervention {

namespace features {

const char kHeavyAdReportingOnlyParamName[] = "reporting-only";
const char kHeavyAdReportingEnabledParamName[] = "reporting-enabled";

// Enables or disables the intervention that unloads ad iframes with intensive
// resource usage.
BASE_FEATURE(kHeavyAdIntervention,
             "HeavyAdIntervention",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables reporting on the intervention that unloads ad iframes
// with intensive resource usage.
BASE_FEATURE(kHeavyAdInterventionWarning,
             "HeavyAdInterventionWarning",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the privacy mitigations for the heavy ad intervention.
// This throttles the amount of interventions that can occur on a given host in
// a time period. It also adds noise to the thresholds used. This is separate
// from the intervention feature so it does not interfere with field trial
// activation, as this blocklist is created for every user, and noise is decided
// prior to seeing a heavy ad.
BASE_FEATURE(kHeavyAdPrivacyMitigations,
             "HeavyAdPrivacyMitigations",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features

}  // namespace heavy_ad_intervention
