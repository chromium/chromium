// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/attribution_scopes_set.h"

#include <stdint.h>

#include <optional>

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::TriggerRegistrationError;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::UnorderedElementsAre;

TEST(AttributionScopesSetTest, ParseSource) {
  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<
        base::expected<AttributionScopesSet, SourceRegistrationError>>
        matches;
    std::optional<uint32_t> scope_limit = 20u;
  } kTestCases[] = {
      {
          "empty",
          R"json({})json",
          ErrorIs(SourceRegistrationError::kAttributionScopesInvalid),
      },
      {
          "empty_null_limit",
          R"json({})json",
          ValueIs(Property(&AttributionScopesSet::scopes, IsEmpty())),
          /*scope_limit=*/std::nullopt,
      },
      {
          "scopes_list_wrong_type",
          R"json({"attribution_scopes": 0})json",
          ErrorIs(SourceRegistrationError::kAttributionScopesInvalid),
      },
      {
          "basic_scopes",
          R"json({"attribution_scopes": ["1"]})json",
          ValueIs(Property(&AttributionScopesSet::scopes,
                           UnorderedElementsAre("1"))),
      },
      {
          "basic_scopes_null_limit",
          R"json({"attribution_scopes": ["1"]})json",
          ErrorIs(SourceRegistrationError::kAttributionScopeLimitRequired),
          /*scope_limit=*/std::nullopt,
      },
      {
          "scopes_list_empty",
          R"json({"attribution_scopes": []})json",
          ErrorIs(SourceRegistrationError::kAttributionScopesInvalid),
      },
      {
          "scopes_list_empty_null_limit",
          R"json({"attribution_scopes": []})json",
          ValueIs(Property(&AttributionScopesSet::scopes, IsEmpty())),
          /*scope_limit=*/std::nullopt,
      },
      {
          "scope_in_list_wrong_type",
          R"json({"attribution_scopes": [1]})json",
          ErrorIs(SourceRegistrationError::kAttributionScopesValueInvalid),
      },
      {
          "multiple_scopes",
          R"json({"attribution_scopes": ["1", "2", "3"]})json",
          ValueIs(Property(&AttributionScopesSet::scopes,
                           UnorderedElementsAre("1", "2", "3"))),
      },
      {
          "too_many_scopes",
          R"json({
            "attribution_scopes": [
              "1", "2", "3", "4", "5"
            ]
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopesInvalid),
          /*scope_limit=*/4,
      },
      {
          "too_many_scopes_default_limit",
          R"json({
            "attribution_scopes": [
              "01", "02", "03", "04", "05",
              "06", "07", "08", "09", "10",
              "11", "12", "13", "14", "15",
              "16", "17", "18", "19", "20", "21"
            ]
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopesInvalid),
          /*scope_limit=*/25,
      },
      {
          "duplicate_scopes_condensed",
          R"json({"attribution_scopes": ["1", "1"]})json",
          ValueIs(Property(&AttributionScopesSet::scopes,
                           UnorderedElementsAre("1"))),
      },
      {
          "scopes_value_length_too_long",
          R"json({"attribution_scopes": [
            "This string exceeds 50 characters to pass the limit"
          ]})json",
          ErrorIs(SourceRegistrationError::kAttributionScopesValueInvalid),
      },
  };

  base::test::ScopedFeatureList scoped_feature_list(
      features::kAttributionScopes);
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    base::Value::Dict value = base::test::ParseJsonDict(test_case.json);
    EXPECT_THAT(AttributionScopesSet::FromJSON(value, test_case.scope_limit),
                test_case.matches);
  }
}

TEST(AttributionScopesSetTest, ParseTrigger) {
  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<
        base::expected<AttributionScopesSet, TriggerRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "empty",
          R"json({})json",
          ValueIs(AttributionScopesSet()),
      },
      {
          "scopes_list_wrong_type",
          R"json({"attribution_scopes": 0})json",
          ErrorIs(TriggerRegistrationError::kAttributionScopesInvalid),
      },
      {
          "basic_scopes",
          R"json({"attribution_scopes": ["1"]})json",
          ValueIs(Property(&AttributionScopesSet::scopes,
                           UnorderedElementsAre("1"))),
      },
      {
          "scopes_list_empty",
          R"json({"attribution_scopes": []})json",
          ValueIs(AttributionScopesSet()),
      },
      {
          "scope_in_list_wrong_type",
          R"json({"attribution_scopes": [1]})json",
          ErrorIs(TriggerRegistrationError::kAttributionScopesValueInvalid),
      },
      {
          "multiple_scopes",
          R"json({"attribution_scopes": ["1", "2", "3"]})json",
          ValueIs(Property(&AttributionScopesSet::scopes,
                           UnorderedElementsAre("1", "2", "3"))),
      },
      {
          "more_than_20_scopes",
          R"json({
            "attribution_scopes": [
              "01", "02", "03", "04", "05",
              "06", "07", "08", "09", "10",
              "11", "12", "13", "14", "15",
              "16", "17", "18", "19", "20", "21"
            ]
          })json",
          ValueIs(Property(
              &AttributionScopesSet::scopes,
              UnorderedElementsAre("01", "02", "03", "04", "05", "06", "07",
                                   "08", "09", "10", "11", "12", "13", "14",
                                   "15", "16", "17", "18", "19", "20", "21"))),
      },
      {
          "duplicate_scopes_condensed",
          R"json({"attribution_scopes": ["1", "1"]})json",
          ValueIs(Property(&AttributionScopesSet::scopes,
                           UnorderedElementsAre("1"))),
      },
      {
          "scope_more_than_50_characters",
          R"json({"attribution_scopes": [
            "This string exceeds 50 characters to pass the limit"
          ]})json",
          ValueIs(Property(
              &AttributionScopesSet::scopes,
              UnorderedElementsAre(
                  "This string exceeds 50 characters to pass the limit"))),
      },
  };

  base::test::ScopedFeatureList scoped_feature_list(
      features::kAttributionScopes);
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    base::Value::Dict value = base::test::ParseJsonDict(test_case.json);
    EXPECT_THAT(AttributionScopesSet::FromJSON(value), test_case.matches);
  }
}

TEST(AttributionScopesSetTest, ToJson) {
  const struct {
    AttributionScopesSet input;
    const char* expected_json;
  } kTestCases[] = {
      {
          AttributionScopesSet(),
          R"json({})json",
      },
      {
          AttributionScopesSet({"foo", "bar"}),
          R"json({"attribution_scopes": ["bar", "foo"]})json",
      },
  };

  base::test::ScopedFeatureList scoped_feature_list(
      features::kAttributionScopes);
  for (const auto& test_case : kTestCases) {
    base::Value::Dict actual;
    test_case.input.Serialize(actual);
    EXPECT_THAT(actual, base::test::IsJson(test_case.expected_json));
  }
}

}  // namespace
}  // namespace attribution_reporting
