// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/aggregation_service/parsing_utils.h"

#include <string>

#include "components/aggregation_service/aggregation_service.mojom.h"
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
      {"aws-cloud", AggregationCoordinator::kAwsCloud},
      {"AWS-CLOUD", absl::nullopt},
      {"unknown", absl::nullopt},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(ParseAggregationCoordinator(test_case.str), test_case.expected);
  }
}

TEST(AggregationServiceParsingUtilsTest, SerializeAggregationCoordinator) {
  const struct {
    mojom::AggregationCoordinator coordinator;
    const char* expected;
  } kTestCases[] = {
      {AggregationCoordinator::kAwsCloud, "aws-cloud"},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(SerializeAggregationCoordinator(test_case.coordinator),
              test_case.expected);
  }
}

}  // namespace
}  // namespace aggregation_service
