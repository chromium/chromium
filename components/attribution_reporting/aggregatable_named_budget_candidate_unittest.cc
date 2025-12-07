// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_named_budget_candidate.h"

#include <optional>

#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;

TEST(AggregatableNamedBudgetCandidateTest, Parse) {
  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<base::expected<AggregatableNamedBudgetCandidate,
                                      TriggerRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "wrong_type",
          R"json([])json",
          ErrorIs(TriggerRegistrationError::kAggregatableNamedBudgetWrongType),
      },
      {
          "empty_dict",
          R"json({})json",
          ValueIs(AggregatableNamedBudgetCandidate(/*name=*/std::nullopt,
                                                   FilterPair())),
      },
      {
          "basic",
          R"json({"name": "a"})json",
          ValueIs(AggregatableNamedBudgetCandidate(/*name=*/"a", FilterPair())),
      },
      {
          "wrong_type",
          R"json({"name": 1})json",
          ErrorIs(
              TriggerRegistrationError::kAggregatableNamedBudgetNameInvalid),
      },
      {
          "basic_with_filters",
          R"json({
            "name": "a",
            "filters": [{
              "c": ["1"]
            }],
            "not_filters": [{
              "d": ["2"]
            }]
          })json",
          ValueIs(AggregatableNamedBudgetCandidate(
              /*name=*/"a",
              FilterPair(
                  /*positive=*/{*FilterConfig::Create({{"c", {"1"}}})},
                  /*negative=*/{*FilterConfig::Create({{"d", {"2"}}})}))),
      },
      {
          "filters_only",
          R"json({
            "filters": [{
              "c": ["1"]
            }],
            "not_filters": [{
              "d": ["2"]
            }]
          })json",
          ValueIs(AggregatableNamedBudgetCandidate(
              /*name=*/std::nullopt,
              FilterPair(
                  /*positive=*/{*FilterConfig::Create({{"c", {"1"}}})},
                  /*negative=*/{*FilterConfig::Create({{"d", {"2"}}})}))),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    base::Value value = base::test::ParseJson(test_case.json);
    EXPECT_THAT(AggregatableNamedBudgetCandidate::FromJSON(value),
                test_case.matches);
  }
}

TEST(AggregatableNamedBudgetCandidateTest, Serialize) {
  const struct {
    AggregatableNamedBudgetCandidate input;
    const char* expected_json;
  } kTestCases[] = {
      {
          AggregatableNamedBudgetCandidate(/*name=*/std::nullopt, FilterPair()),
          R"json({})json",
      },
      {
          AggregatableNamedBudgetCandidate(/*name=*/"a", FilterPair()),
          R"json({
            "name": "a",
          })json",
      },
      {
          AggregatableNamedBudgetCandidate(
              /*name=*/"a",
              FilterPair(/*positive=*/{*FilterConfig::Create({{"c", {}}})},
                         /*negative=*/{*FilterConfig::Create({{"d", {}}})})),
          R"json({
            "name": "a",
            "filters": [{"c": []}],
            "not_filters": [{"d": []}],
          })json",
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_THAT(test_case.input.ToJson(),
                base::test::IsJson(test_case.expected_json));
  }
}

}  // namespace
}  // namespace attribution_reporting
