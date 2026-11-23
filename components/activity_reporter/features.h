// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTIVITY_REPORTER_FEATURES_H_
#define COMPONENTS_ACTIVITY_REPORTER_FEATURES_H_

#include "base/feature.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace activity_reporter::features {

BASE_DECLARE_FEATURE(kActivityReporterPingFreshnessRotation);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kPingFreshnessRotationMaxAge);

// Returns the age at which the reporter replaces the ping freshness:
// `kPingFreshnessRotationMaxAge` clamped to between 24 hours and 28 days, or
// `update_client::kPingFreshnessMaxAge` if the param isn't positive.
base::TimeDelta GetPingFreshnessRotationMaxAge();

}  // namespace activity_reporter::features

#endif  // COMPONENTS_ACTIVITY_REPORTER_FEATURES_H_
