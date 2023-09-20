// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregation_keys.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

TEST(AggregationKeysTest, FromJSON) {
  const struct {
    const char* description;
    absl::optional<base::Value> json;
    base::expected<AggregationKeys, SourceRegistrationError> expected;
  } kTestCases[] = {
      {
          "Null",
          absl::nullopt,
          AggregationKeys(),
      },
      {
          "Not a dictionary",
          base::Value(base::Value::List()),
          base::unexpected(SourceRegistrationError::kAggregationKeysWrongType),
      },
      {
          "key not a string",
          base::test::ParseJson(R"({"key":123})"),
          base::unexpected(
              SourceRegistrationError::kAggregationKeysValueWrongType),
      },
      {
          "key doesn't start with 0x",
          base::test::ParseJson(R"({"key":"159"})"),
          base::unexpected(
              SourceRegistrationError::kAggregationKeysValueWrongFormat),
      },
      {
          "Invalid key",
          base::test::ParseJson(R"({"key":"0xG59"})"),
          base::unexpected(
              SourceRegistrationError::kAggregationKeysValueWrongFormat),
      },
      {
          "One valid key",
          base::test::ParseJson(R"({"key":"0x159"})"),
          *AggregationKeys::FromKeys(
              {{"key", absl::MakeUint128(/*high=*/0, /*low=*/345)}}),
      },
      {"Two valid keys",
       base::test::ParseJson(
           R"({"key1":"0x159","key2":"0x50000000000000159"})"),
       *AggregationKeys::FromKeys({
           {
               "key1",
               absl::MakeUint128(/*high=*/0, /*low=*/345),
           },
           {
               "key2",
               absl::MakeUint128(/*high=*/5, /*low=*/345),
           },
       })},
      {
          "Second key invalid",
          base::test::ParseJson(R"({"key1":"0x159","key2":""})"),
          base::unexpected(
              SourceRegistrationError::kAggregationKeysValueWrongFormat),
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(AggregationKeys::FromJSON(base::OptionalToPtr(test_case.json)),
              test_case.expected)
        << test_case.description;
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

    absl::optional<AggregationKeys> Expected() const {
      if (!valid)
        return absl::nullopt;

      AggregationKeys::Keys keys;
      for (size_t i = 0u; i < key_count; ++i) {
        keys.emplace(GetKey(i), absl::MakeUint128(/*high=*/0, /*low=*/1));
      }

      return *AggregationKeys::FromKeys(std::move(keys));
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
      {"max_key_size", true, 1, kMaxBytesPerAggregationKeyId},
      {"excessive_key_size", false, 1, kMaxBytesPerAggregationKeyId + 1},
  };

  for (const auto& test_case : kTestCases) {
    base::Value value(test_case.GetHeader());
    EXPECT_EQ(AggregationKeys::FromJSON(&value).has_value(),
              test_case.Expected().has_value())
        << test_case.description;
  }
}

TEST(AggregationKeysTest, FromJSON_RecordsMetric) {
  using ::base::Bucket;
  using ::testing::ElementsAre;

  absl::optional<base::Value> json = base::test::ParseJson(R"json({
    "a": "0x3",
    "b": "0x4"
  })json");
  ASSERT_TRUE(json);

  base::HistogramTester histograms;
  ASSERT_TRUE(AggregationKeys::FromJSON(base::OptionalToPtr(json)).has_value());

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
