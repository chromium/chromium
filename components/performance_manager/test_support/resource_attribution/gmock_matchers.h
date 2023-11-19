// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_GMOCK_MATCHERS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_GMOCK_MATCHERS_H_

#include "components/performance_manager/public/resource_attribution/query_results.h"
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

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_GMOCK_MATCHERS_H_
