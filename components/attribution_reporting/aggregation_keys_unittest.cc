// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregation_keys.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Property;

TEST(AggregationKeysTest, FromJSON) {
  EXPECT_THAT(AggregationKeys::FromJSON(nullptr),
              ValueIs(Property(&AggregationKeys::keys, IsEmpty())));

  const struct {
    const char* description;
    const char* json;
    ::testing::Matcher<base::expected<AggregationKeys, SourceRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "Not a dictionary",
          "[]",
          ErrorIs(SourceRegistrationError::kAggregationKeysDictInvalid),
      },
      {
          "key not a string",
          R"({"key":123})",
          ErrorIs(SourceRegistrationError::kAggregationKeysValueInvalid),
      },
      {
          "key doesn't start with 0x",
          R"({"key":"159"})",
          ErrorIs(SourceRegistrationError::kAggregationKeysValueInvalid),
      },
      {
          "Invalid key",
          R"({"key":"0xG59"})",
          ErrorIs(SourceRegistrationError::kAggregationKeysValueInvalid),
      },
      {
          "One valid key",
          R"({"key":"0x159"})",
          ValueIs(Property(
              &AggregationKeys::keys,
              ElementsAre(
                  Pair("key", absl::MakeUint128(/*high=*/0, /*low=*/345))))),
      },
      {
          "Two valid keys",
          R"({"key1":"0x159","key2":"0x50000000000000159"})",
          ValueIs(Property(
              &AggregationKeys::keys,
              ElementsAre(
                  Pair("key1", absl::MakeUint128(/*high=*/0, /*low=*/345)),
                  Pair("key2", absl::MakeUint128(/*high=*/5, /*low=*/345))))),
      },
      {
          "Second key invalid",
          R"({"key1":"0x159","key2":""})",
          ErrorIs(SourceRegistrationError::kAggregationKeysValueInvalid),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    base::Value value = base::test::ParseJson(test_case.json);
    EXPECT_THAT(AggregationKeys::FromJSON(&value), test_case.matches);
  }
}

TEST(AggregationKeysTest, FromJSON_CheckSize) {
  struct TestCase {
    const char* description;
    bool valid;
    size_t key_count;
    size_t key_size;

    base::Value::Dict GetHeader() const {
      base::Value::Dict dict;
      for (size_t i = 0u; i < key_count; ++i) {
        dict.Set(GetKey(i), "0x1");
      }
      return dict;
    }

   private:
    std::string GetKey(size_t index) const {
      // Note that this might not be robust as
      // `attribution_reporting::kMaxAggregationKeysPerSource` varies
      // which might generate invalid JSON.
      return std::string(key_size, 'A' + index % 26 + 32 * (index / 26));
    }
  };

  const TestCase kTestCases[] = {
      {"empty", true, 0, 0},
      {"max_keys", true, kMaxAggregationKeysPerSource, 1},
      {"too_many_keys", false, kMaxAggregationKeysPerSource + 1, 1},
      {"max_key_size", true, 1, AggregationKeys::kMaxBytesPerAggregationKeyId},
      {"excessive_key_size", false, 1,
       AggregationKeys::kMaxBytesPerAggregationKeyId + 1},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    base::Value value(test_case.GetHeader());
    EXPECT_EQ(AggregationKeys::FromJSON(&value).has_value(), test_case.valid);
  }
}

TEST(AggregationKeysTest, FromJSON_RecordsMetric) {
  using ::base::Bucket;

  base::Value json = base::test::ParseJson(R"json({
    "a": "0x3",
    "b": "0x4"
  })json");

  base::HistogramTester histograms;
  ASSERT_TRUE(AggregationKeys::FromJSON(&json).has_value());

  EXPECT_THAT(histograms.GetAllSamples("Conversions.AggregatableKeysPerSource"),
              ElementsAre(Bucket(2, 1)));
}

TEST(AggregationKeysTest, ToJson) {
  const struct {
    AggregationKeys input;
    const char* expected_json;
  } kTestCases[] = {
      {
          AggregationKeys(),
          R"json({})json",
      },
      {
          *AggregationKeys::FromKeys({{"a", 1}, {"b", 15}}),
          R"json({"a":"0x1","b":"0xf"})json",
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_THAT(test_case.input.ToJson(),
                base::test::IsJson(test_case.expected_json));
  }
}

}  // namespace
}  // namespace attribution_reporting
