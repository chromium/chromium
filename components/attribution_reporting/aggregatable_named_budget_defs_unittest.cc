// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_named_budget_defs.h"

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;

TEST(AggregatableNamedBudgetDefsTest, Parse) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAttributionAggregatableNamedBudgets);
  EXPECT_THAT(AggregatableNamedBudgetDefs::FromJSON(/*value=*/nullptr),
              ValueIs(AggregatableNamedBudgetDefs()));
  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<
        base::expected<AggregatableNamedBudgetDefs, SourceRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "wrong_type",
          R"json([])json",
          ErrorIs(
              SourceRegistrationError::kAggregatableNamedBudgetsDictInvalid),
      },
      {
          "empty_dict",
          R"json({})json",
          ValueIs(AggregatableNamedBudgetDefs()),
      },
      {
          "basic",
          R"json({"a": 65536})json",
          ValueIs(AggregatableNamedBudgetDefs::FromBudgetMap({{"a", 65536}})),
      },
      {
          "value_wrong_type",
          R"json({"a": "20"})json",
          ErrorIs(
              SourceRegistrationError::kAggregatableNamedBudgetsValueInvalid),
      },
      {
          "key_too_long",
          R"json({
            "This string exceeds 25 chars": 20
          })json",
          ErrorIs(SourceRegistrationError::kAggregatableNamedBudgetsKeyTooLong),
      },
      {
          "zero_budget",
          R"json({"a": 0})json",
          ValueIs(AggregatableNamedBudgetDefs::FromBudgetMap({{"a", 0}})),
      },
      {
          "negative_budget",
          R"json({"a": -1})json",
          ErrorIs(
              SourceRegistrationError::kAggregatableNamedBudgetsValueInvalid),
      },
      {
          "non_int_budget",
          R"json({"a": 1.1})json",
          ErrorIs(
              SourceRegistrationError::kAggregatableNamedBudgetsValueInvalid),
      },
      {
          "trailing_zero_budget",
          R"json({"a": 1.0})json",
          ValueIs(AggregatableNamedBudgetDefs::FromBudgetMap({{"a", 1}})),
      },
      {
          "exceeds_budget_limit",
          R"json({"a": 65537})json",
          ErrorIs(
              SourceRegistrationError::kAggregatableNamedBudgetsValueInvalid),
      },
      {
          "too_many_keys",
          R"json({
            "a": 1,
            "b": 1,
            "c": 1,
            "d": 1,
            "e": 1,
            "f": 1,
            "g": 1,
            "h": 1,
            "i": 1,
            "j": 1,
            "k": 1,
            "l": 1,
            "m": 1,
            "n": 1,
            "o": 1,
            "p": 1,
            "q": 1,
            "r": 1,
            "s": 1,
            "t": 1,
            "u": 1,
            "v": 1,
            "w": 1,
            "x": 1,
            "y": 1,
            "z": 1,
          })json",
          ErrorIs(
              SourceRegistrationError::kAggregatableNamedBudgetsDictInvalid),
      },
      {
          "values_combined_exceed_limit",
          R"json({"a": 65536, "b": 1})json",
          ValueIs(AggregatableNamedBudgetDefs::FromBudgetMap(
              {{"a", 65536}, {"b", 1}})),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    base::Value value = base::test::ParseJson(test_case.json);
    EXPECT_THAT(AggregatableNamedBudgetDefs::FromJSON(&value),
                test_case.matches);
  }
}

TEST(AggregatableNamedBudgetDefsTest, Serialize) {
  const struct {
    AggregatableNamedBudgetDefs input;
    const char* expected_json;
  } kTestCases[] = {
      {
          *AggregatableNamedBudgetDefs::FromBudgetMap({{"a", 1}, {"b", 50}}),
          R"json({
            "named_budgets": {
              "a": 1,
              "b": 50
          }})json",
      },
      {
          AggregatableNamedBudgetDefs(),
          R"json({})json",
      },
  };

  for (const auto& test_case : kTestCases) {
    base::Value::Dict dict;
    test_case.input.Serialize(dict);
    EXPECT_THAT(dict, base::test::IsJson(test_case.expected_json));
  }
}

}  // namespace
}  // namespace attribution_reporting
