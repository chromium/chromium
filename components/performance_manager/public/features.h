// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header contains field trial and variations definitions for policies,
// mechanisms and features in the performance_manager component.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FEATURES_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace performance_manager {
namespace features {

// The feature that gates the TabLoadingFrameNavigationPolicy, and its
// mechanism counterpart the TabLoadingFrameNavigationScheduler.
extern const base::Feature kTabLoadingFrameNavigationThrottles;

// Parameters controlling the TabLoadingFrameNavigationThrottles feature.
struct TabLoadingFrameNavigationThrottlesParams {
  TabLoadingFrameNavigationThrottlesParams();
  ~TabLoadingFrameNavigationThrottlesParams();

  static TabLoadingFrameNavigationThrottlesParams GetParams();

  // The minimum and maximum amount of time throttles will be applied to
  // non-primary content frames.
  base::TimeDelta minimum_throttle_timeout;
  base::TimeDelta maximum_throttle_timeout;

  // The multiple of elapsed time from navigation start until
  // FirstContentfulPaint (FCP) that is used in calculating the timeout to apply
  // to the throttles.
  double fcp_multiple;
};

// The feature that gates whether or not the PM runs on the main (UI) thread.
extern const base::Feature kRunOnMainThread;

#if !defined(OS_ANDROID)
// Enables urgent discarding of pages directly from PerformanceManager rather
// than via TabManager.
extern const base::Feature kUrgentDiscardingFromPerformanceManager;

// The discard strategy to use.
// Integer values are specified to allow conversion from the integer value in
// the DiscardStrategy feature param.
enum class DiscardStrategy : int {
  // Discards the least recently used tab among the eligible ones. This is the
  // default strategy.
  LRU = 0,
  // Discard the tab with the biggest resident set among the eligible ones.
  BIGGEST_RSS = 1,
};

class UrgentDiscardingParams {
 public:
  ~UrgentDiscardingParams();

  static UrgentDiscardingParams GetParams();

  DiscardStrategy discard_strategy() const { return discard_strategy_; }

  static constexpr base::FeatureParam<int> kDiscardStrategy{
      &features::kUrgentDiscardingFromPerformanceManager, "DiscardStrategy",
      static_cast<int>(DiscardStrategy::LRU)};

 private:
  UrgentDiscardingParams();
  UrgentDiscardingParams(const UrgentDiscardingParams& rhs);

  DiscardStrategy discard_strategy_;
};

// Feature that controls whether or not tabs should be automatically discarded
// when the total PMF is too high.
extern const base::Feature kHighPMFDiscardPolicy;

// Enable background tab loading of pages (restored via session restore)
// directly from Performance Manager rather than via TabLoader.
extern const base::Feature kBackgroundTabLoadingFromPerformanceManager;
#endif

// Policy that evicts the BFCache of pages that become non visible or the
// BFCache of all pages when the system is under memory pressure.
extern const base::Feature kBFCachePerformanceManagerPolicy;

// Parameters allowing to control some aspects of the
// |kBFCachePerformanceManagerPolicy|.
class BFCachePerformanceManagerPolicyParams {
 public:
  ~BFCachePerformanceManagerPolicyParams();

  static BFCachePerformanceManagerPolicyParams GetParams();

  // Whether or not the BFCache of all pages should be flushed when the system
  // is under *moderate* memory pressure. The policy always flushes the bfcache
  // under critical pressure.
  bool flush_on_moderate_pressure() const {
    return flush_on_moderate_pressure_;
  }

  static constexpr base::FeatureParam<bool> kFlushOnModeratePressure{
      &features::kBFCachePerformanceManagerPolicy, "flush_on_moderate_pressure",
      true};

 private:
  BFCachePerformanceManagerPolicyParams();
  BFCachePerformanceManagerPolicyParams(
      const BFCachePerformanceManagerPolicyParams& rhs);

  bool flush_on_moderate_pressure_ = true;
};

}  // namespace features
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FEATURES_H_
