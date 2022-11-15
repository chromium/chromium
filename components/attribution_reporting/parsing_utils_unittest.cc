// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/parsing_utils.h"

#include <string>

#include "components/attribution_reporting/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {
namespace {

TEST(AggregationKeyUtilsTest, StringToAggregationKeyPiece) {
  const struct {
    const char* string;
    absl::optional<absl::uint128> expected;
  } kTestCases[] = {
      {"123", absl::nullopt},
      {"0x123", 291},
      {"0X123", 291},
      {"0xG", absl::nullopt},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(StringToAggregationKeyPiece(test_case.string),
              test_case.expected);
  }
}

TEST(AggregationKeyUtilsTest, AggregationKeyIdHasValidLength) {
  EXPECT_TRUE(AggregationKeyIdHasValidLength(
      std::string(kMaxBytesPerAggregationKeyId, 'a')));
  EXPECT_FALSE(AggregationKeyIdHasValidLength(
      std::string(kMaxBytesPerAggregationKeyId + 1, 'a')));
}

}  // namespace
}  // namespace attribution_reporting
