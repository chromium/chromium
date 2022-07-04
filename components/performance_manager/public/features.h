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

namespace performance_manager::features {

// The feature that gates whether or not the PM runs on the main (UI) thread.
extern const base::Feature kRunOnMainThread;

#if !BUILDFLAG(IS_ANDROID)
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

// Enable background tab loading of pages (restored via session restore)
// directly from Performance Manager rather than via TabLoader.
extern const base::Feature kBackgroundTabLoadingFromPerformanceManager;

// Make the High-Efficiency or Battery Saver Modes available to users. If this
// is enabled, it doesn't mean the specific Mode is enabled, just that the user
// has the option of toggling it.
extern const base::Feature kHighEfficiencyModeAvailable;
extern const base::Feature kBatterySaverModeAvailable;

// Defines the time in seconds before a background tab is discarded for
// High-Efficiency Mode.
extern const base::FeatureParam<base::TimeDelta>
    kHighEfficiencyModeTimeBeforeDiscard;
#endif

// Policy that evicts the BFCache of pages that become non visible or the
// BFCache of all pages when the system is under memory pressure.
extern const base::Feature kBFCachePerformanceManagerPolicy;

}  // namespace performance_manager::features

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FEATURES_H_
