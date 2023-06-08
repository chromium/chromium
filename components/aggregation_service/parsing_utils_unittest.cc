// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/aggregation_service/parsing_utils.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/aggregation_service/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace aggregation_service {
namespace {

using ::aggregation_service::mojom::AggregationCoordinator;

TEST(AggregationServiceParsingUtilsTest, ParseAggregationCoordinator) {
  const struct {
    std::string str;
    absl::optional<mojom::AggregationCoordinator> expected;
  } kTestCases[] = {
      {kAggregationServiceCoordinatorAwsCloud.Get(),
       AggregationCoordinator::kAwsCloud},
      {"https://a.test", absl::nullopt},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(ParseAggregationCoordinator(test_case.str), test_case.expected);
  }
}

TEST(AggregationServiceParsingUtilsTest, SerializeAggregationCoordinator) {
  const struct {
    mojom::AggregationCoordinator coordinator;
    std::string expected;
  } kTestCases[] = {
      {AggregationCoordinator::kAwsCloud,
       kAggregationServiceCoordinatorAwsCloud.Get()},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(SerializeAggregationCoordinator(test_case.coordinator),
              test_case.expected);
  }
}

}  // namespace
}  // namespace aggregation_service
