// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/aggregation_service/parsing_utils.h"

#include <optional>
#include <string>

#include "components/aggregation_service/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace aggregation_service {
namespace {

TEST(AggregationServiceParsingUtilsTest, ParseAggregationCoordinator) {
  const struct {
    std::string str;
    std::optional<url::Origin> expected;
  } kTestCases[] = {
      {kAggregationServiceCoordinatorAwsCloud.Get(),
       url::Origin::Create(GURL(kAggregationServiceCoordinatorAwsCloud.Get()))},
      {"https://a.test", std::nullopt},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(ParseAggregationCoordinator(test_case.str), test_case.expected)
        << test_case.str;
  }
}

}  // namespace
}  // namespace aggregation_service
