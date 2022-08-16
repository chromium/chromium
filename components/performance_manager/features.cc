// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header contains field trial and variations definitions for policies,
// mechanisms and features in the performance_manager component.

#include "components/performance_manager/public/features.h"

#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace performance_manager::features {

const base::Feature kRunOnMainThread{"RunOnMainThread",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

#if !BUILDFLAG(IS_ANDROID)
const base::Feature kBackgroundTabLoadingFromPerformanceManager{
    "BackgroundTabLoadingFromPerformanceManager",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kHighEfficiencyModeAvailable{
    "HighEfficiencyModeAvailable", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBatterySaverModeAvailable{
    "BatterySaverModeAvailable", base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<base::TimeDelta> kHighEfficiencyModeTimeBeforeDiscard{
    &kHighEfficiencyModeAvailable, "time_before_discard", base::Hours(2)};

extern const base::FeatureParam<bool> kHighEfficiencyModeDefaultState{
    &kHighEfficiencyModeAvailable, "default_state", false};
#endif

const base::Feature kBFCachePerformanceManagerPolicy{
    "BFCachePerformanceManagerPolicy", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace performance_manager::features
