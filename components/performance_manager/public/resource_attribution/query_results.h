// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_QUERY_RESULTS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_QUERY_RESULTS_H_

#include <compare>
#include <map>
#include <optional>

#include "base/time/time.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"

namespace resource_attribution {

// The Resource Attribution result and metadata structs described in
// https://bit.ly/resource-attribution-api#heading=h.k8fjwkwxxdj6.

// The methods used to produce a result.
enum class MeasurementAlgorithm {
  // The values in this result were measured directly, such as with an
  // OS system call.
  kDirectMeasurement,

  // The values in this result are estimates derived by subdividing direct
  // measurements of a context between several related contexts, such as with
  // SplitResourceAmongFramesAndWorkers().
  kSplit,

  // The values in this result are estimates derived by summing kSplit estimates
  // over a collection of contexts.
  kSum,
};

// Metadata about the measurement that produced a result.
struct ResultMetadata {
  // The time this measurement was taken.
  base::TimeTicks measurement_time;

  // Method used to assign measurement results to the resource context.
  MeasurementAlgorithm algorithm;

  // Constructor ensures both `measurement_time` and `algorithm` are set.
  //
  // Since there's no default constructor, any ResultType class containing
  // metadata also can't be default-constructed. This ensures none of them have
  // an invalid or uninitialized state. Use std::optional<ResultType> when
  // default-construction is needed.
  ResultMetadata(base::TimeTicks measurement_time,
                 MeasurementAlgorithm algorithm)
      : measurement_time(measurement_time), algorithm(algorithm) {}

  friend constexpr auto operator<=>(const ResultMetadata&,
                                    const ResultMetadata&) = default;
  friend constexpr bool operator==(const ResultMetadata&,
                                   const ResultMetadata&) = default;
};

// The result of a kCPUTime query.
struct CPUTimeResult {
  ResultMetadata metadata;

  // The time that Resource Attribution started monitoring the CPU usage of this
  // context.
  base::TimeTicks start_time;

  // Total time the context spent on CPU between `start_time` and
  // `metadata.measurement_time`.
  //
  // `cumulative_cpu` / (`metadata.measurement_time` - `start_time`)
  // gives percentage of CPU used as a fraction in the range 0% to 100% *
  // SysInfo::NumberOfProcessors(), the same as
  // ProcessMetrics::GetPlatformIndependentCPUUsage().
  base::TimeDelta cumulative_cpu;

  // Total time the context spent on CPU in a background process between
  // `start_time` and `metadata.measurement_time`. Time spent on CPU in a
  // foreground process doesn't affect this, even if the context itself was
  // backgrounded.
  base::TimeDelta cumulative_background_cpu;

  friend constexpr auto operator<=>(const CPUTimeResult&,
                                    const CPUTimeResult&) = default;
  friend constexpr bool operator==(const CPUTimeResult&,
                                   const CPUTimeResult&) = default;
};

// Results of a kMemorySummary query.
struct MemorySummaryResult {
  ResultMetadata metadata;
  uint64_t resident_set_size_kb = 0;
  uint64_t private_footprint_kb = 0;

  friend constexpr auto operator<=>(const MemorySummaryResult&,
                                    const MemorySummaryResult&) = default;
  friend constexpr bool operator==(const MemorySummaryResult&,
                                   const MemorySummaryResult&) = default;
};

// A container for at most one of each query result type. This is not a variant
// because it can contain more than one result.
struct QueryResults {
  std::optional<CPUTimeResult> cpu_time_result;
  std::optional<MemorySummaryResult> memory_summary_result;

  friend constexpr auto operator<=>(const QueryResults&,
                                    const QueryResults&) = default;
  friend constexpr bool operator==(const QueryResults&,
                                   const QueryResults&) = default;
};

// A map from a ResourceContext to all query results received for that context.
using QueryResultMap = std::map<ResourceContext, QueryResults>;

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_QUERY_RESULTS_H_
