// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/parsing_utils.h"

#include <stdint.h>

#include <limits>
#include <string>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {
namespace {

TEST(AttributionReportingParsingUtilsTest, StringToAggregationKeyPiece) {
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

TEST(AttributionReportingParsingUtilsTest, AggregationKeyIdHasValidLength) {
  EXPECT_TRUE(AggregationKeyIdHasValidLength(
      std::string(kMaxBytesPerAggregationKeyId, 'a')));
  EXPECT_FALSE(AggregationKeyIdHasValidLength(
      std::string(kMaxBytesPerAggregationKeyId + 1, 'a')));
}

TEST(AttributionReportingParsingUtilsTest, ParseUint64) {
  const struct {
    const char* description;
    const char* json;
    absl::optional<uint64_t> expected;
  } kTestCases[] = {
      {
          "missing_key",
          R"json({})json",
          absl::nullopt,
      },
      {
          "not_string",
          R"json({"key":123})json",
          absl::nullopt,
      },
      {
          "negative",
          R"json({"key":"-1"})json",
          absl::nullopt,
      },
      {
          "zero",
          R"json({"key":"0"})json",
          0,
      },
      {
          "max",
          R"json({"key":"18446744073709551615"})json",
          std::numeric_limits<uint64_t>::max(),
      },
      {
          "out_of_range",
          R"json({"key":"18446744073709551616"})json",
          absl::nullopt,
      },
  };

  for (const auto& test_case : kTestCases) {
    base::Value value = base::test::ParseJson(test_case.json);
    EXPECT_EQ(ParseUint64(value.GetDict(), "key"), test_case.expected)
        << test_case.description;
  }
}

TEST(AttributionReportingParsingUtilsTest, ParseInt64) {
  const struct {
    const char* description;
    const char* json;
    absl::optional<int64_t> expected;
  } kTestCases[] = {
      {
          "missing_key",
          R"json({})json",
          absl::nullopt,
      },
      {
          "not_string",
          R"json({"key":123})json",
          absl::nullopt,
      },
      {
          "zero",
          R"json({"key":"0"})json",
          0,
      },
      {
          "min",
          R"json({"key":"-9223372036854775808"})json",
          std::numeric_limits<int64_t>::min(),
      },
      {
          "max",
          R"json({"key":"9223372036854775807"})json",
          std::numeric_limits<int64_t>::max(),
      },
      {
          "out_of_range",
          R"json({"key":"9223372036854775808"})json",
          absl::nullopt,
      },
  };

  for (const auto& test_case : kTestCases) {
    base::Value value = base::test::ParseJson(test_case.json);
    EXPECT_EQ(ParseInt64(value.GetDict(), "key"), test_case.expected)
        << test_case.description;
  }
}

TEST(AttributionReportingParsingUtilsTest, HexEncodeAggregationKey) {
  const struct {
    absl::uint128 input;
    const char* expected;
  } kTestCases[] = {
      {0, "0x0"},
      {absl::MakeUint128(/*high=*/0,
                         /*low=*/std::numeric_limits<uint64_t>::max()),
       "0xffffffffffffffff"},
      {absl::MakeUint128(/*high=*/1,
                         /*low=*/std::numeric_limits<uint64_t>::max()),
       "0x1ffffffffffffffff"},
      {std::numeric_limits<absl::uint128>::max(),
       "0xffffffffffffffffffffffffffffffff"},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(HexEncodeAggregationKey(test_case.input), test_case.expected)
        << test_case.input;
  }
}

}  // namespace
}  // namespace attribution_reporting
