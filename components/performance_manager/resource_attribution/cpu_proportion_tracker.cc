// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/cpu_proportion_tracker.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"

namespace resource_attribution {

CPUProportionTracker::CPUProportionTracker(
    ContextFilterCallback context_filter,
    CPUProportionType cpu_proportion_type)
    : cpu_proportion_type_(cpu_proportion_type),
      context_filter_(std::move(context_filter)) {}

CPUProportionTracker::~CPUProportionTracker() = default;

void CPUProportionTracker::StartFirstInterval(base::TimeTicks time,
                                              const QueryResultMap& results) {
  CHECK(!last_measurement_time_.has_value());
  CHECK(cached_cpu_measurements_.empty());
  last_measurement_time_ = time;
  cached_cpu_measurements_ = results;
}

std::map<ResourceContext, double> CPUProportionTracker::StartNextInterval(
    base::TimeTicks time,
    const QueryResultMap& results) {
  CHECK(last_measurement_time_.has_value());
  const base::TimeTicks interval_start = last_measurement_time_.value();
  const base::TimeDelta measurement_interval = time - interval_start;
  last_measurement_time_ = time;
  if (measurement_interval.is_zero()) {
    // No time passed to measure. Ignore the results to avoid division by zero.
    return {};
  }
  CHECK(measurement_interval.is_positive());

  // Swap a new measurement into `cached_cpu_measurements_`, storing the
  // previous contents in `previous_measurements`.
  QueryResultMap previous_measurements =
      std::exchange(cached_cpu_measurements_, results);

  std::map<ResourceContext, double> cpu_usage_map;
  for (const auto& [context, result] : cached_cpu_measurements_) {
    if (!context_filter_.is_null() && !context_filter_.Run(context)) {
      continue;
    }
    if (!result.cpu_time_result.has_value()) {
      continue;
    }

    // Let time A be the last time StartNextInterval() was called, or the time
    // when StartFirstInterval() was called if this is the first one. The
    // results seen at that time are saved in `previous_measurements`.
    //
    // Let time B be current time. (`measurement_interval` is A..B.)
    //
    // There are 5 cases:
    //
    // 1. The context was created at time C, between A and B. (It will not be
    // found in `previous_measurements`).
    //
    // This snapshot should include 0% CPU for time A..C, and the measured % of
    // CPU for time C..B.
    //
    // A    C         B
    // |----+---------|
    // | 0% |   X%    |
    //
    // CPU(C..B) is `result.cumulative_cpu`.
    // `result.start_time` is C.
    // `result.metadata.measurement_time` is B.
    //
    // 2. The context existed for the entire duration A..B.
    //
    // This snapshot should include the measured % of CPU for the whole time
    // A..B.
    //
    // A              B
    // |--------------|
    // |      X%      |
    //
    // CPU(A..B) is `result.cumulative_cpu -
    // previous_measurements[context].cumulative_cpu`.
    // `result.start_time` <= A.
    // `result.metadata.measurement_time` is B.
    //
    // 3. Context created before time A, exited at time D, between A and B.
    //
    // The snapshot should include the measured % of CPU for time A..D, and 0%
    // CPU for time D..B.
    //
    // A         D    B
    // |---------+----|
    // |    X%   | 0% |
    //
    // CPU(A..D) is `result.cumulative_cpu -
    // previous_measurements[context].cumulative_cpu`.
    // `result.start_time` <= A.
    // `result.metadata.measurement_time` is D.
    //
    // 4. Context created at time C and exited at time D, both between A and B.
    // (context is not found in `previous_measurements`.
    // `result.cumulative_cpu` ends at time D, which is
    // `result.metadata.measurement_time`.)
    //
    // The snapshot should include the measured % of CPU for time C..D, and 0%
    // CPU for the rest.
    //
    // A    C    D    B
    // |----+----+----|
    // | 0% | X% | 0% |
    //
    // CPU(C..D) is `result.cumulative_cpu`.
    // `result.start_time` is C.
    // `result.metadata.measurement_time` is D.
    //
    // 5. Context exited before time A. (This is an old cached result.)
    //
    // The snapshot should not include this context at all.
    //
    // C    D A              B
    // |----| |--------------|
    // | X% | |      0%      |
    // `result.start_time` <= `result.metadata.measurement_time` <= A
    if (result.cpu_time_result->metadata.measurement_time < interval_start) {
      // Case 5.
      continue;
    }
    base::TimeDelta current_cpu = GetCumulativeCPU(*result.cpu_time_result);
    if (result.cpu_time_result->start_time < interval_start) {
      // Case 2 or 3.
      const auto it = previous_measurements.find(context);
      if (it == previous_measurements.end() ||
          !it->second.cpu_time_result.has_value()) {
        // No baseline to know how much of the context's CPU came before the
        // interval. Skip it.
        continue;
      }
      current_cpu -= GetCumulativeCPU(*it->second.cpu_time_result);
    }
    CHECK(!current_cpu.is_negative());
    cpu_usage_map.emplace(context, current_cpu / measurement_interval);
  }
  return cpu_usage_map;
}

void CPUProportionTracker::Stop() {
  CHECK(last_measurement_time_.has_value());
  last_measurement_time_.reset();
  cached_cpu_measurements_.clear();
}

bool CPUProportionTracker::IsTracking() const {
  return last_measurement_time_.has_value();
}

base::TimeDelta CPUProportionTracker::GetCumulativeCPU(
    const CPUTimeResult& cpu_time_result) const {
  switch (cpu_proportion_type_) {
    case CPUProportionType::kAll:
      return cpu_time_result.cumulative_cpu;
    case CPUProportionType::kBackground:
      return cpu_time_result.cumulative_background_cpu;
  }
}

}  // namespace resource_attribution
