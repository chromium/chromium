// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIVERSAL_OPTOUT_FEATURES_H_
#define COMPONENTS_UNIVERSAL_OPTOUT_FEATURES_H_

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace universal_optout::features {

// Controls the rollout of the Universal Opt-Out feature.
BASE_DECLARE_FEATURE(kUniversalOptOut);

// Controls the rollout of the Universal Opt-Out extension on supported iOS
// versions (iOS 18.4 to 26).
BASE_DECLARE_FEATURE(kUniversalOptOutExtension);

// Feature flag for the settings page development.
BASE_DECLARE_FEATURE(kUniversalOptOutSettings);

// Returns whether the Universal Opt-Out feature is enabled.
// On non-iOS platforms, returns `kUniversalOptOut`.
// On iOS < 18.4, returns false.
// On iOS >= 27, returns `kUniversalOptOut`.
// On iOS between 18.4 and 27, returns true only if both `kUniversalOptOut` and
// `kUniversalOptOutExtension` are enabled.
bool IsUniversalOptOutEnabled();

// Comma-separated list of target administrative area codes that are eligible
// (e.g., "us-fl,us-tx").
extern const base::FeatureParam<std::string> kTargetLocations;

// Number of days in the sliding window used to determine eligibility.
extern const base::FeatureParam<base::TimeDelta> kEligibilityWindow;

// Number of days in the sliding window used to determine trailing eligibility.
extern const base::FeatureParam<base::TimeDelta> kTrailingEligibilityWindow;

// Returns the list of target locations parsed from `kTargetLocations`.
std::vector<std::string> GetTargetLocations();

}  // namespace universal_optout::features

#endif  // COMPONENTS_UNIVERSAL_OPTOUT_FEATURES_H_
