// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_values.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Property;

TEST(AggregatableValuesTest, Parse) {
  EXPECT_THAT(AggregatableValues::FromJSON(nullptr),
              ValueIs(Property(&AggregatableValues::values, IsEmpty())));

  const struct {
    const char* description;
    const char* json;
    ::testing::Matcher<
        base::expected<AggregatableValues, TriggerRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "empty",
          R"json({})json",
          ValueIs(Property(&AggregatableValues::values, IsEmpty())),
      },
      {
          "not_dictionary",
          R"json([])json",
          ErrorIs(TriggerRegistrationError::kAggregatableValuesWrongType),
      },
      {
          "value_not_int",
          R"json({"a": true})json",
          ErrorIs(TriggerRegistrationError::kAggregatableValuesValueWrongType),
      },
      {
          "value_below_range",
          R"json({"a": 0})json",
          ErrorIs(TriggerRegistrationError::kAggregatableValuesValueOutOfRange),
      },
      {
          "value_above_range",
          R"json({"a": 65537})json",
          ErrorIs(TriggerRegistrationError::kAggregatableValuesValueOutOfRange),
      },
      {
          "valid",
          R"json({"a": 1, "b": 65536})json",
          ValueIs(Property(&AggregatableValues::values,
                           ElementsAre(Pair("a", 1), Pair("b", 65536)))),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    base::Value value = base::test::ParseJson(test_case.json);
    EXPECT_THAT(AggregatableValues::FromJSON(&value), test_case.matches);
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
    EXPECT_THAT(parse_dict_with_key_length(length), ValueIs(_));
  }

  EXPECT_THAT(parse_dict_with_key_length(26),
              ErrorIs(TriggerRegistrationError::kAggregatableValuesKeyTooLong));
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
