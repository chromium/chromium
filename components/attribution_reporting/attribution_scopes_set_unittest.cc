// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/attribution_scopes_set.h"

#include <stdint.h>

#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
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
    uint32_t scope_limit = 20u;
  } kTestCases[] = {
      {
          "empty",
          R"json({})json",
          ErrorIs(SourceRegistrationError::kAttributionScopesListInvalid),
      },
      {
          "scopes_list_wrong_type",
          R"json({"values": 0})json",
          ErrorIs(SourceRegistrationError::kAttributionScopesListInvalid),
      },
      {
          "basic_scopes",
          R"json({"values": ["1"]})json",
          ValueIs(Property(&AttributionScopesSet::scopes,
                           UnorderedElementsAre("1"))),
      },
      {
          "scopes_list_empty",
          R"json({"values": []})json",
          ErrorIs(SourceRegistrationError::kAttributionScopesListInvalid),
      },
      {
          "scope_in_list_wrong_type",
          R"json({"values": [1]})json",
          ErrorIs(SourceRegistrationError::kAttributionScopesListValueInvalid),
      },
      {
          "multiple_scopes",
          R"json({"values": ["1", "2", "3"]})json",
          ValueIs(Property(&AttributionScopesSet::scopes,
                           UnorderedElementsAre("1", "2", "3"))),
      },
      {
          "too_many_scopes",
          R"json({
            "values": [
              "1", "2", "3", "4", "5"
            ]
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopesListInvalid),
          /*scope_limit=*/4,
      },
      {
          "too_many_scopes_default_limit",
          R"json({
            "values": [
              "01", "02", "03", "04", "05",
              "06", "07", "08", "09", "10",
              "11", "12", "13", "14", "15",
              "16", "17", "18", "19", "20", "21"
            ]
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopesListInvalid),
          /*scope_limit=*/25,
      },
      {
          "duplicate_scopes_condensed",
          R"json({"values": ["1", "1"]})json",
          ValueIs(Property(&AttributionScopesSet::scopes,
                           UnorderedElementsAre("1"))),
      },
      {
          "scopes_value_length_too_long",
          R"json({"values": [
            "This string exceeds 50 characters to pass the limit"
          ]})json",
          ErrorIs(SourceRegistrationError::kAttributionScopesListValueInvalid),
      },
  };

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

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    base::Value::Dict value = base::test::ParseJsonDict(test_case.json);
    EXPECT_THAT(AttributionScopesSet::FromJSON(value), test_case.matches);
  }
}

TEST(AttributionScopesSetTest, ToJsonSource) {
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
          R"json({"values": ["bar", "foo"]})json",
      },
  };

  for (const auto& test_case : kTestCases) {
    base::Value::Dict actual;
    test_case.input.SerializeForSource(actual);
    EXPECT_THAT(actual, base::test::IsJson(test_case.expected_json));
  }
}

TEST(AttributionScopesSetTest, ToJsonTrigger) {
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

  for (const auto& test_case : kTestCases) {
    base::Value::Dict actual;
    test_case.input.SerializeForTrigger(actual);
    EXPECT_THAT(actual, base::test::IsJson(test_case.expected_json));
  }
}

TEST(AttributionScopesSetTest, HasIntersection) {
  const struct {
    AttributionScopesSet set_1;
    AttributionScopesSet set_2;
    bool expected;
  } kTestCases[] = {
      {AttributionScopesSet(), AttributionScopesSet(), false},
      {AttributionScopesSet({"a"}), AttributionScopesSet(), false},
      {AttributionScopesSet({"a"}), AttributionScopesSet({"a"}), true},
      {AttributionScopesSet({"a", "c"}), AttributionScopesSet({"b", "c"}),
       true},
      {AttributionScopesSet({"a"}), AttributionScopesSet({"b"}), false}};
  for (const auto& test_case : kTestCases) {
    EXPECT_THAT(test_case.set_1.HasIntersection(test_case.set_2),
                test_case.expected);
    EXPECT_THAT(test_case.set_2.HasIntersection(test_case.set_1),
                test_case.expected);
  }
}

}  // namespace
}  // namespace attribution_reporting
