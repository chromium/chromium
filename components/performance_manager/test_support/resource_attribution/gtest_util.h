// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_GTEST_UTIL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_GTEST_UTIL_H_

#include <optional>
#include <ostream>

#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_attribution {

namespace internal {

// Returns a reference to the QueryResults member holding a ResultType.
template <typename ResultType>
constexpr const std::optional<ResultType>& QueryResultsMember(
    const QueryResults& results);

template <>
constexpr const std::optional<CPUTimeResult>& QueryResultsMember(
    const QueryResults& results) {
  return results.cpu_time_result;
}

template <>
constexpr const std::optional<MemorySummaryResult>& QueryResultsMember(
    const QueryResults& results) {
  return results.memory_summary_result;
}

// Returns the name of a type at compile-time, for better error messages.
template <typename T>
constexpr const char* TypeNameString();

template <>
constexpr const char* TypeNameString<CPUTimeResult>() {
  return "CPUTimeResult";
}

template <>
constexpr const char* TypeNameString<MemorySummaryResult>() {
  return "MemorySummaryResult";
}

}  // namespace internal

// gMock matcher expecting that a given ResultType object (which can be any
// object with a ResultMetadata field) has a ResultMetadata member whose fields
// match `measurement_time_matcher` and `algorithm_matcher`.
template <typename ResultType, typename Matcher1, typename Matcher2>
auto ResultMetadataMatches(Matcher1 measurement_time_matcher,
                           Matcher2 algorithm_matcher) {
  return ::testing::Field(
      "metadata", &ResultType::metadata,
      ::testing::AllOf(::testing::Field("measurement_time",
                                        &ResultMetadata::measurement_time,
                                        measurement_time_matcher),
                       ::testing::Field("algorithm", &ResultMetadata::algorithm,
                                        algorithm_matcher)));
}

// gMock matcher expecting that a given QueryResults object contains a result of
// type ResultType that matches `matcher`.
//
// Usage:
//
//  // Match any CPUTimeResult:
//  EXPECT_THAT(results, QueryResultsMatch<CPUTimeResult>(_));
//
//  // Match an exact MemorySummaryResult:
//  EXPECT_THAT(results, QueryResultsMatch<MemorySummaryResult>(
//                           MemorySummaryResult{...});
template <typename ResultType, typename Matcher>
auto QueryResultsMatch(Matcher matcher) {
  return ::testing::ResultOf(
      // Error messages will be formatted "whose ResultType value..."
      internal::TypeNameString<ResultType>(),
      &internal::QueryResultsMember<ResultType>, ::testing::Optional(matcher));
}

// As QueryResultsMatch() but expects that the QueryResults object contains
// multiple results.
//
// Usage:
//
//  // Match an exact MemorySummaryResult and any CPUTimeResult:
//  EXPECT_THAT(results,
//              QueryResultsMatchAll<MemorySummaryResult, CPUTimeResult>(
//                  MemorySummaryResult{...}, _));
template <typename... ResultTypes, typename... Matchers>
auto QueryResultsMatchAll(Matchers... matchers) {
  return ::testing::AllOf(QueryResultsMatch<ResultTypes>(matchers)...);
}

// gMock matcher expecting that a given QueryResultMap entry has a key of
// `resource_context` whose value contains a result of type ResultType that
// matches `matcher`.
//
// Usage:
//
//   QueryResultMap result_map;
//   result_map[context1] = {CPUTimeResult{...}};
//   result_map[context2] = {CPUTimeResult{...}, MemorySummaryResult{...}};
//
//   // Tests that the map contains `context1` and possibly other elements.
//   EXPECT_THAT(result_map, ::testing::Contains(
//       ResultForContextMatches<CPUTimeResult>(context1, _));
//
//   // Tests that the map contains exactly `context1` and `context2`.
//   // `context2` contains at least a MemorySummaryResult and possibly others.
//   EXPECT_THAT(result_map, ::testing::UnorderedElementsAre(
//       ResultForContextMatches<CPUTimeResult>(context1, _),
//       ResultForContextMatches<MemorySummaryResult>(context2, _)));
template <typename ResultType, typename Matcher>
auto ResultForContextMatches(const ResourceContext& resource_context,
                             Matcher matcher) {
  // Pair matches the key and value of a map entry.
  return ::testing::Pair(resource_context,
                         QueryResultsMatch<ResultType>(matcher));
}

// As ResultForContextMatches() but expects that the QueryResultMap entry
// contains multiple results.
//
// Usage:
//
//   QueryResultMap result_map;
//   result_map[context1] = {CPUTimeResult{...}};
//   result_map[context2] = {CPUTimeResult{...}, MemorySummaryResult{...}};
//
//   // Tests that the map contains exactly `context1` and `context2`.
//   // `context2` contains at a CPUTimeResult and a MemorySummaryResult.
//   EXPECT_THAT(result_map, ::testing::UnorderedElementsAre(
//       ResultForContextMatches<CPUTimeResult>(context1, _),
//       ResultForContextMatchesAll<CPUTimeResult, MemorySummaryResult>(
//           context2, _, _)));
template <typename... ResultTypes, typename... Matchers>
auto ResultForContextMatchesAll(const ResourceContext& resource_context,
                                Matchers... matchers) {
  // Pair matches the key and value of a map entry.
  return ::testing::Pair(
      resource_context,
      ::testing::AllOf(QueryResultsMatch<ResultTypes>(matchers)...));
}

// Test result printers. These need to go in the same namespace as the type
// being printed.

inline void PrintTo(MeasurementAlgorithm algorithm, std::ostream* os) {
  switch (algorithm) {
    case MeasurementAlgorithm::kDirectMeasurement:
      *os << "DirectMeasurement";
      return;
    case MeasurementAlgorithm::kSplit:
      *os << "Split";
      return;
    case MeasurementAlgorithm::kSum:
      *os << "Sum";
      return;
  }
  *os << "UnknownAlgorithm:" << static_cast<int>(algorithm);
}

inline void PrintTo(const ResultMetadata& metadata, std::ostream* os) {
  *os << "measurement_time:" << metadata.measurement_time
      << ",algorithm:" << ::testing::PrintToString(metadata.algorithm);
}

inline void PrintTo(const CPUTimeResult& result, std::ostream* os) {
  *os << "cumulative_cpu:" << result.cumulative_cpu
      << ",cumulative_background_cpu:" << result.cumulative_background_cpu
      << ",start_time:" << result.start_time
      << ",metadata:" << ::testing::PrintToString(result.metadata) << " ("
      << (result.metadata.measurement_time - result.start_time) << ")";
}

inline void PrintTo(const MemorySummaryResult& result, std::ostream* os) {
  *os << "rss:" << result.resident_set_size_kb
      << ",pmf:" << result.private_footprint_kb
      << ",metadata:" << ::testing::PrintToString(result.metadata);
}

// PrintTo() matches the generic absl::variant overload before ResourceContext,
// so each context type needs a separate printer.

inline void PrintTo(const FrameContext& context, std::ostream* os) {
  *os << context.ToString();
}

inline void PrintTo(const PageContext& context, std::ostream* os) {
  *os << context.ToString();
}

inline void PrintTo(const ProcessContext& context, std::ostream* os) {
  *os << context.ToString();
}

inline void PrintTo(const WorkerContext& context, std::ostream* os) {
  *os << context.ToString();
}

inline void PrintTo(const OriginInBrowsingInstanceContext& context,
                    std::ostream* os) {
  *os << context.ToString();
}

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_GTEST_UTIL_H_
