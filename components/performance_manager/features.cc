// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header contains field trial and variations definitions for policies,
// mechanisms and features in the performance_manager component.

#include "components/performance_manager/public/features.h"

#include "base/dcheck_is_on.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace performance_manager {
namespace features {

const base::Feature kTabLoadingFrameNavigationThrottles{
    "TabLoadingFrameNavigationThrottles", base::FEATURE_DISABLED_BY_DEFAULT};

// Parameters associated with the "TabLoadingFrameNavigationThrottles"
// feature.
const base::FeatureParam<int> kMinimumThrottleTimeoutMilliseconds = {
    &kTabLoadingFrameNavigationThrottles, "MinimumThrottleTimeoutMilliseconds",
    1000};
// This defaults to the 99th %ile of LargestContentfulPaint (LCP).
const base::FeatureParam<int> kMaximumThrottleTimeoutMilliseconds = {
    &kTabLoadingFrameNavigationThrottles, "MaximumThrottleTimeoutMilliseconds",
    40000};
// This defaults to 3 since 3 * 99th%ile FCP ~= 99th%ile LCP.
const base::FeatureParam<double> kFCPMultiple = {
    &kTabLoadingFrameNavigationThrottles, "FCPMultiple", 3.0};

TabLoadingFrameNavigationThrottlesParams::
    TabLoadingFrameNavigationThrottlesParams() = default;

TabLoadingFrameNavigationThrottlesParams::
    ~TabLoadingFrameNavigationThrottlesParams() = default;

// static
TabLoadingFrameNavigationThrottlesParams
TabLoadingFrameNavigationThrottlesParams::GetParams() {
  TabLoadingFrameNavigationThrottlesParams params;
  params.minimum_throttle_timeout = base::TimeDelta::FromMilliseconds(
      kMinimumThrottleTimeoutMilliseconds.Get());
  params.maximum_throttle_timeout = base::TimeDelta::FromMilliseconds(
      kMaximumThrottleTimeoutMilliseconds.Get());
  params.fcp_multiple = kFCPMultiple.Get();
  return params;
}

const base::Feature kRunOnMainThread{"RunOnMainThread",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

#if !defined(OS_ANDROID)
const base::Feature kUrgentDiscardingFromPerformanceManager {
  "UrgentDiscardingFromPerformanceManager",
// Chrome OS uses memory pressure evaluator instead of performance manager to
// discard tabs.
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

UrgentDiscardingParams::UrgentDiscardingParams() = default;
UrgentDiscardingParams::UrgentDiscardingParams(
    const UrgentDiscardingParams& rhs) = default;
UrgentDiscardingParams::~UrgentDiscardingParams() = default;

constexpr base::FeatureParam<int> UrgentDiscardingParams::kDiscardStrategy;

// static
UrgentDiscardingParams UrgentDiscardingParams::GetParams() {
  UrgentDiscardingParams params = {};
  params.discard_strategy_ = static_cast<DiscardStrategy>(
      UrgentDiscardingParams::kDiscardStrategy.Get());
  return params;
}

const base::Feature kBackgroundTabLoadingFromPerformanceManager{
    "BackgroundTabLoadingFromPerformanceManager",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kHighPMFDiscardPolicy{"HighPMFDiscardPolicy",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const base::Feature kBFCachePerformanceManagerPolicy{
    "BFCachePerformanceManagerPolicy", base::FEATURE_DISABLED_BY_DEFAULT};

BFCachePerformanceManagerPolicyParams::BFCachePerformanceManagerPolicyParams() =
    default;
BFCachePerformanceManagerPolicyParams::BFCachePerformanceManagerPolicyParams(
    const BFCachePerformanceManagerPolicyParams& rhs) = default;
BFCachePerformanceManagerPolicyParams::
    ~BFCachePerformanceManagerPolicyParams() = default;

constexpr base::FeatureParam<bool>
    BFCachePerformanceManagerPolicyParams::kFlushOnModeratePressure;

// static
BFCachePerformanceManagerPolicyParams
BFCachePerformanceManagerPolicyParams::GetParams() {
  BFCachePerformanceManagerPolicyParams params = {};
  params.flush_on_moderate_pressure_ =
      BFCachePerformanceManagerPolicyParams::kFlushOnModeratePressure.Get();
  return params;
}

}  // namespace features
}  // namespace performance_manager
