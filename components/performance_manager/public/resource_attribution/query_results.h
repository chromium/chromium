// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_QUERY_RESULTS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_QUERY_RESULTS_H_

#include <map>
#include <vector>

#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/public/resource_attribution/type_helpers.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace performance_manager::resource_attribution {

// The Resource Attribution result and metadata structs described in
// https://bit.ly/resource-attribution-api#heading=h.k8fjwkwxxdj6.

// TODO(crbug.com/1471683): Add MeasurementAlgorithm to metadata

// Metadata about the measurement that produced a result.
struct ResultMetadata {
  // The time this measurement was taken.
  base::TimeTicks measurement_time;
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
};

// Results of a kMemorySummary query.
struct MemorySummaryResult {
  ResultMetadata metadata;
  uint64_t resident_set_size_kb = 0;
  uint64_t private_footprint_kb = 0;
};

using QueryResult = absl::variant<CPUTimeResult, MemorySummaryResult>;
using QueryResults = std::vector<QueryResult>;
using QueryResultMap = std::map<ResourceContext, QueryResults>;

// Returns true iff `results` contains any result of type T.
template <typename T>
constexpr bool ContainsResult(const QueryResults& results) {
  return internal::VariantVectorContains<T>(results);
}

// If `results` contains any result of type T, returns a reference to that
// result. Otherwise, returns nullopt.
//
// Note that a non-const ref can't be returned from a const QueryResults. The
// following uses are valid:
//
//   base::optional_ref<CPUTimeResult> result =
//       AsResult<CPUTimeResult>(mutable_query_results);
//
//   base::optional_ref<const CPUTimeResult> result =
//       AsResult<CPUTimeResult>(const_query_results);
//
//   base::optional_ref<const CPUTimeResult> result =
//       AsResult<const CPUTimeResult>(const_or_mutable_query_results);
//
// To make a copy of the result, use one of:
//
//    absl::optional<T> result = AsResult<T>(query_results).CopyAsOptional();
//    T result = AsResult<T>(query_results).value();  // Crashes on nullopt.
template <typename T>
constexpr base::optional_ref<T> AsResult(QueryResults& results) {
  return internal::GetFromVariantVector<T>(results);
}
template <typename T>
constexpr base::optional_ref<const T> AsResult(const QueryResults& results) {
  return internal::GetFromVariantVector<T>(results);
}

inline bool operator==(const ResultMetadata& a, const ResultMetadata& b) {
  static_assert(sizeof(ResultMetadata) ==
                    sizeof(decltype(ResultMetadata::measurement_time)),
                "update operator== when changing ResultMetadata");
  return a.measurement_time == b.measurement_time;
}

inline bool operator!=(const ResultMetadata& a, const ResultMetadata& b) {
  return !(a == b);
}

inline bool operator==(const CPUTimeResult& a, const CPUTimeResult& b) {
  static_assert(sizeof(CPUTimeResult) ==
                    sizeof(decltype(CPUTimeResult::metadata)) +
                        sizeof(decltype(CPUTimeResult::start_time)) +
                        sizeof(decltype(CPUTimeResult::cumulative_cpu)),
                "update operator== when changing CPUTimeResult");
  return a.metadata == b.metadata && a.start_time == b.start_time &&
         a.cumulative_cpu == b.cumulative_cpu;
}

inline bool operator!=(const CPUTimeResult& a, const CPUTimeResult& b) {
  return !(a == b);
}

inline bool operator==(const MemorySummaryResult& a,
                       const MemorySummaryResult& b) {
  static_assert(
      sizeof(MemorySummaryResult) ==
          sizeof(decltype(MemorySummaryResult::metadata)) +
              sizeof(decltype(MemorySummaryResult::resident_set_size_kb)) +
              sizeof(decltype(MemorySummaryResult::private_footprint_kb)),
      "update operator== when changing MemorySummaryResult");
  return a.metadata == b.metadata &&
         a.resident_set_size_kb == b.resident_set_size_kb &&
         a.private_footprint_kb == b.private_footprint_kb;
}

inline bool operator!=(const MemorySummaryResult& a,
                       const MemorySummaryResult& b) {
  return !(a == b);
}

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_QUERY_RESULTS_H_
