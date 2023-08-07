// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_values.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

TEST(AggregatableValuesTest, Parse) {
  const struct {
    const char* description;
    absl::optional<base::Value> json;
    base::expected<AggregatableValues, TriggerRegistrationError> expected;
  } kTestCases[] = {
      {
          "null",
          absl::nullopt,
          AggregatableValues(),
      },
      {
          "empty",
          base::Value(base::Value::Dict()),
          AggregatableValues(),
      },
      {
          "not_dictionary",
          base::Value(base::Value::List()),
          base::unexpected(
              TriggerRegistrationError::kAggregatableValuesWrongType),
      },
      {
          "value_not_int",
          base::test::ParseJson(R"json({"a": true})json"),
          base::unexpected(
              TriggerRegistrationError::kAggregatableValuesValueWrongType),
      },
      {
          "value_below_range",
          base::test::ParseJson(R"json({"a": 0})json"),
          base::unexpected(
              TriggerRegistrationError::kAggregatableValuesValueOutOfRange),
      },
      {
          "value_above_range",
          base::test::ParseJson(R"json({"a": 65537})json"),
          base::unexpected(
              TriggerRegistrationError::kAggregatableValuesValueOutOfRange),
      },
      {
          "valid",
          base::test::ParseJson(R"json({"a": 1, "b": 65536})json"),
          *AggregatableValues::Create({
              {"a", 1},
              {"b", 65536},
          }),
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(AggregatableValues::FromJSON(base::OptionalToPtr(test_case.json)),
              test_case.expected)
        << test_case.description;
  }
}

TEST(AggregatableValuesTest, Parse_KeyLength) {
  auto parse_dict_with_key_length = [](size_t length) {
    base::Value::Dict dict;
    dict.Set(std::string(length, 'a'), 1);
    base::Value value(std::move(dict));
    return AggregatableValues::FromJSON(&value);
  };

  for (size_t length = 0; length < 26; length++) {
    EXPECT_TRUE(parse_dict_with_key_length(length).has_value());
  }

  EXPECT_THAT(parse_dict_with_key_length(26),
              base::test::ErrorIs(
                  TriggerRegistrationError::kAggregatableValuesKeyTooLong));
}

TEST(AggregatableValuesTest, Parse_KeyCount) {
  auto parse_dict_with_key_count = [](size_t count) {
    base::Value::Dict dict;
    for (size_t i = 0; i < count; i++) {
      dict.Set(base::NumberToString(i), 1);
    }
    base::Value value(std::move(dict));
    return AggregatableValues::FromJSON(&value);
  };

  for (size_t count = 0; count <= kMaxAggregationKeysPerSourceOrTrigger;
       count++) {
    EXPECT_TRUE(parse_dict_with_key_count(count).has_value());
  }

  EXPECT_THAT(
      parse_dict_with_key_count(kMaxAggregationKeysPerSourceOrTrigger + 1),
      base::test::ErrorIs(
          TriggerRegistrationError::kAggregatableValuesTooManyKeys));
}

TEST(AggregatableValuesTest, ToJson) {
  const struct {
    AggregatableValues input;
    const char* expected_json;
  } kTestCases[] = {
      {
          AggregatableValues(),
          R"json({})json",
      },
      {
          *AggregatableValues::Create({{"a", 1}, {"b", 2}}),
          R"json({"a":1,"b":2})json",
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_THAT(test_case.input.ToJson(),
                base::test::IsJson(test_case.expected_json));
  }
}

}  // namespace
}  // namespace attribution_reporting
