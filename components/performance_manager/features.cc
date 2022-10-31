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

BASE_FEATURE(kRunOnMainThread,
             "RunOnMainThread",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kBackgroundTabLoadingFromPerformanceManager,
             "BackgroundTabLoadingFromPerformanceManager",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHighEfficiencyModeAvailable,
             "HighEfficiencyModeAvailable",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBatterySaverModeAvailable,
             "BatterySaverModeAvailable",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

const base::FeatureParam<int> kHighEfficiencyModePromoMemoryPercentThreshold{
    &kHighEfficiencyModeAvailable,
    "memory_percent_threshold",
    70,
};

// On ChromeOS, the adjustment generally seems to be around 3%, sometimes 2%. We
// choose 3% because it gets us close enough, or overestimates (which is better
// than underestimating in this instance).
const base::FeatureParam<int>
    kBatterySaverModeThresholdAdjustmentForDisplayLevel {
  &kBatterySaverModeAvailable, "low_battery_threshold_adjustment",
#if BUILDFLAG(IS_CHROMEOS_ASH)
      3,
#else
      0,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};
#endif

BASE_FEATURE(kBFCachePerformanceManagerPolicy,
             "BFCachePerformanceManagerPolicy",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUrgentPageDiscarding,
             "UrgentPageDiscarding",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPageTimelineMonitor,
             "PageTimelineMonitor",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kPageTimelineStateIntervalTime{
    &kPageTimelineMonitor, "time_between_collect_slice", base::Minutes(5)};

}  // namespace performance_manager::features
