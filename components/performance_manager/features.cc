// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header contains field trial and variations definitions for policies,
// mechanisms and features in the performance_manager component.

#include "components/performance_manager/public/features.h"

#include "base/feature_list.h"
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

const base::FeatureParam<bool> kHighEfficiencyModeDefaultState{
    &kHighEfficiencyModeAvailable, "default_state", false};

// 10 tabs is the 70th percentile of tab counts based on UMA data.
const base::FeatureParam<int> kHighEfficiencyModePromoTabCountThreshold{
    &kHighEfficiencyModeAvailable,
    "tab_count_threshold",
    10,
};
#endif

const base::Feature kBFCachePerformanceManagerPolicy{
    "BFCachePerformanceManagerPolicy", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUrgentPageDiscarding{"UrgentPageDiscarding",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace performance_manager::features
