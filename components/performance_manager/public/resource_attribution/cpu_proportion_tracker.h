// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_CPU_PROPORTION_TRACKER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_CPU_PROPORTION_TRACKER_H_

#include <map>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"

namespace resource_attribution {

// A class that, given a series of consecutive CPUTimeResult measurements, will
// calculate the proportion of CPU used over a series of consecutive intervals.
// Any part of the interval that isn't covered by the measurements is counted as
// 0% CPU.
class CPUProportionTracker {
 public:
  enum class CPUProportionType {
    // Calculate CPU proportion based on `CPUTimeResult::cumulative_cpu`.
    kAll,
    // Calculate CPU proportion based on
    // `CPUTimeResult::cumulative_background_cpu`.
    kBackground,
  };

  // A callback that's called for every context in the QueryResultMap passed
  // to StartNextInterval(). If it returns false, the context is ignored.
  // This can be used when the caller only wants to calculate the proportion
  // for a subset of results from a query, to avoid making a filtered copy of
  // the QueryResultMap every interval.
  using ContextFilterCallback =
      base::RepeatingCallback<bool(const ResourceContext&)>;

  explicit CPUProportionTracker(
      ContextFilterCallback context_filter = base::NullCallback(),
      CPUProportionType cpu_proportion_type = CPUProportionType::kAll);
  ~CPUProportionTracker();

  CPUProportionTracker(const CPUProportionTracker&) = delete;
  CPUProportionTracker operator=(const CPUProportionTracker&) = delete;

  // Starts tracking the proportion of CPU. Must be called exactly once before
  // calling StartNextInterval(). `results` is the result of a CPU query taken
  // at `time`. Saves CPUTimeResult::cumulative_cpu for each context in
  // `results` as a baseline of CPU used before the start of the interval.
  void StartFirstInterval(base::TimeTicks time, const QueryResultMap& results);

  // Calculates the proportion of CPU used by all contexts in `results` since
  // the last call to StartFirstInterval() or StartNextInterval(). `results` is
  // the result of a CPU query taken at `time`.
  //
  // Returns a map from ResourceContext to CPU time used
  // (CPUTimeResult::cumulative_cpu) as a proportion of the interval duration.
  // Since the CPU time can include processes running simultaneously on multiple
  // cores, this can exceed 100%. It's roughly in the range 0% to
  // SysInfo::NumberOfProcessors() * 100%, the same scale as
  // ProcessMetrics::GetPlatformIndependentCPUUsage().
  //
  // If the context's lifetime (from CPUTimeResult::start_time to
  // ResultMetadata::measurement_time) doesn't cover the entire interval, any
  // part that isn't covered is counted as 0%.
  //
  // If the context's `start_time` is before the start of the interval, the
  // part of its `cumulative_cpu` that came before the interval start is
  // excluded. If this context wasn't seen in the last call to
  // StartFirstInterval() or StartNextInterval(), it isn't possible to know how
  // much to exclude, so the context won't be included in the results until the
  // next interval.
  std::map<ResourceContext, double> StartNextInterval(
      base::TimeTicks time,
      const QueryResultMap& results);

  // Clears all state. After calling this StartFirstInterval() must be called
  // again to start a new set of intervals.
  void Stop();

  // Returns true iff StartFirstInterval() was called and Stop() was not.
  bool IsTracking() const;

 private:
  // Extracts the correct cumulative CPU time measurement from
  // `cpu_time_result`, based on `cpu_proportion_type_`.
  base::TimeDelta GetCumulativeCPU(const CPUTimeResult& cpu_time_result) const;

  const CPUProportionType cpu_proportion_type_;

  // Last time CPU measurements were taken (for calculating the total length of
  // a measurement interval).
  std::optional<base::TimeTicks> last_measurement_time_;

  // A map caching the most recent measurements for each context.
  QueryResultMap cached_cpu_measurements_;

  // If not null, called for each context passed to StartNextInterval().
  ContextFilterCallback context_filter_;
};

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_CPU_PROPORTION_TRACKER_H_
