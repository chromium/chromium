// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_GTEST_UTIL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_GTEST_UTIL_H_

#include <ostream>

#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::resource_attribution {

// GMock matcher expecting that a given QueryResultMap entry has a key of
// `resource_context` whose value contains a result of type ResultType.
//
// Usage:
//
//   // Tests that the map contains `context1` and possibly other elements.
//   EXPECT_THAT(result_map, ::testing::Contains(
//       QueryResultMapEntryMatches<CPUTimeResult>(context1));
//
//   // Tests that the map contains exactly `context1` and `context2`.
//   EXPECT_THAT(result_map, ::testing::UnorderedElementsAre(
//       QueryResultMapEntryMatches<CPUTimeResult>(context1),
//       QueryResultMapEntryMatches<CPUTimeResult>(context2));
template <typename ResultType>
auto QueryResultMapEntryMatches(const ResourceContext& resource_context) {
  return ::testing::Pair(
      resource_context,
      ::testing::ResultOf("contains correct QueryResult type",
                          ContainsResult<ResultType>, ::testing::IsTrue()));
}

// Test result printers. These need to go in the same namespace as the type
// being printed.

inline void PrintTo(const ResultMetadata& metadata, std::ostream* os) {
  *os << "measurement_time:" << metadata.measurement_time;
}

inline void PrintTo(const CPUTimeResult& result, std::ostream* os) {
  *os << "cpu:" << result.cumulative_cpu << ",start_time:" << result.start_time
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

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_GTEST_UTIL_H_
