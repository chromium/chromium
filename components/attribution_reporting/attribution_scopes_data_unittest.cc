// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/attribution_scopes_data.h"

#include <optional>

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/attribution_scopes_set.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::AllOf;
using ::testing::Property;

TEST(AttributionScopesDataTest, Parse) {
  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<
        base::expected<AttributionScopesData, SourceRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "empty",
          R"json({})json",
          ValueIs(
              AllOf(Property(&AttributionScopesData::attribution_scope_limit,
                             std::nullopt),
                    Property(&AttributionScopesData::max_event_states,
                             kDefaultMaxEventStates),
                    Property(&AttributionScopesData::attribution_scopes_set,
                             AttributionScopesSet()))),
      },
      {
          "scope_limit_wrong_type",
          R"json({"attribution_scope_limit": "1"})json",
          ErrorIs(SourceRegistrationError::kAttributionScopeLimitInvalid),
      },
      {
          "scope_limit_zero",
          R"json({
            "attribution_scope_limit": 0
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopeLimitInvalid),
      },
      {
          "scope_limit_double",
          R"json({
            "attribution_scope_limit": 1.5
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopeLimitInvalid),
      },
      {
          "scope_limit_negative",
          R"json({
            "attribution_scope_limit": -1
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopeLimitInvalid),
      },
      {
          "scope_limit_greater_than_uint32",
          R"json({
            "attribution_scope_limit": 4294967296
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopeLimitInvalid),
      },
      {
          "scope_limit_only",
          R"json({"attribution_scope_limit": 1})json",
          ErrorIs(SourceRegistrationError::kAttributionScopesInvalid),
      },
      {
          "event_states_wrong_type",
          R"json({
            "max_event_states": "1"
          })json",
          ErrorIs(SourceRegistrationError::kMaxEventStatesInvalid),
      },
      {
          "event_states_zero",
          R"json({
            "max_event_states": 0
          })json",
          ErrorIs(SourceRegistrationError::kMaxEventStatesInvalid),
      },
      {
          "event_states_double",
          R"json({
            "max_event_states": 1.5
          })json",
          ErrorIs(SourceRegistrationError::kMaxEventStatesInvalid),
      },
      {
          "event_states_negative",
          R"json({
            "max_event_states": -1
          })json",
          ErrorIs(SourceRegistrationError::kMaxEventStatesInvalid),
      },
      {
          "event_states_greater_than_uint32",
          R"json({
            "max_event_states": 4294967296
          })json",
          ErrorIs(SourceRegistrationError::kMaxEventStatesInvalid),
      },
      {
          "event_states_valid_no_scope_limit",
          R"json({"max_event_states": 1})json",
          ErrorIs(SourceRegistrationError::kAttributionScopeLimitRequired),
      },
      {
          "scope_limit_and_event_states_valid",
          R"json({
            "attribution_scope_limit": 1,
            "max_event_states": 1
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopesInvalid),
      },
      {
          "empty_scopes_no_scope_limit",
          R"json({"attribution_scopes": []})json",
          ValueIs(
              AllOf(Property(&AttributionScopesData::attribution_scope_limit,
                             std::nullopt),
                    Property(&AttributionScopesData::max_event_states,
                             kDefaultMaxEventStates),
                    Property(&AttributionScopesData::attribution_scopes_set,
                             AttributionScopesSet()))),
      },
      {
          "scopes_no_scope_limit",
          R"json({"attribution_scopes": ["1"]})json",
          ErrorIs(SourceRegistrationError::kAttributionScopeLimitRequired),
      },
      {
          "scopes_wrong_type",
          R"json({
            "attribution_scopes": "1"
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopesInvalid),
      },
      {
          "scope_limit_and_scopes",
          R"json({
            "attribution_scope_limit": 1,
            "attribution_scopes": ["1"]
          })json",
          ValueIs(AllOf(
              Property(&AttributionScopesData::attribution_scope_limit, 1u),
              Property(&AttributionScopesData::max_event_states,
                       kDefaultMaxEventStates),
              Property(&AttributionScopesData::attribution_scopes_set,
                       AttributionScopesSet({"1"})))),
      },
      {
          "scopes_exceed_limit",
          R"json({
            "attribution_scope_limit": 1,
            "attribution_scopes": ["1", "2"]
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopesInvalid),
      },
      {
          "all_fields",
          R"json({
            "attribution_scope_limit": 1,
            "max_event_states": 1,
            "attribution_scopes": ["1"]
          })json",
          ValueIs(AllOf(
              Property(&AttributionScopesData::attribution_scope_limit, 1u),
              Property(&AttributionScopesData::max_event_states, 1u),
              Property(&AttributionScopesData::attribution_scopes_set,
                       AttributionScopesSet({"1"})))),
      },
      {
          "scope_limit_uint32_max",
          R"json({
            "attribution_scope_limit": 4294967295,
            "attribution_scopes": ["1"]
          })json",
          ValueIs(
              AllOf(Property(&AttributionScopesData::attribution_scope_limit,
                             std::numeric_limits<uint32_t>::max()),
                    Property(&AttributionScopesData::max_event_states,
                             kDefaultMaxEventStates),
                    Property(&AttributionScopesData::attribution_scopes_set,
                             AttributionScopesSet({"1"})))),
      },
      {
          "scope_limit_above_uint32_max",
          R"json({
            "attribution_scope_limit": 4294967296,
            "attribution_scopes": ["1"]
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopeLimitInvalid),
      },
      {
          "event_states_uint32_max",
          R"json({
            "attribution_scope_limit": 1,
            "max_event_states": 4294967295,
            "attribution_scopes": ["1"]
          })json",
          ValueIs(AllOf(
              Property(&AttributionScopesData::attribution_scope_limit, 1u),
              Property(&AttributionScopesData::max_event_states,
                       std::numeric_limits<uint32_t>::max()),
              Property(&AttributionScopesData::attribution_scopes_set,
                       AttributionScopesSet({"1"})))),
      },
      {
          "event_states_above_uint32_max",
          R"json({
            "attribution_scope_limit": 1,
            "max_event_states": 4294967296,
            "attribution_scopes": ["1"]
          })json",
          ErrorIs(SourceRegistrationError::kMaxEventStatesInvalid),
      },
  };

  base::test::ScopedFeatureList scoped_feature_list(
      features::kAttributionScopes);

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    base::Value::Dict value = base::test::ParseJsonDict(test_case.json);
    EXPECT_THAT(AttributionScopesData::FromJSON(value), test_case.matches);
  }
}

TEST(AttributionScopesDataTest, Serialize) {
  const struct {
    AttributionScopesData input;
    const char* expected_json;
  } kTestCases[] = {
      {
          AttributionScopesData(),
          R"json({"max_event_states": 3})json",
      },
      {
          *AttributionScopesData::Create(AttributionScopesSet({"foo"}),
                                         /*attribution_scope_limit=*/1u,
                                         /*max_event_states=*/1u),
          R"json({
            "attribution_scope_limit": 1,
            "max_event_states": 1,
            "attribution_scopes": ["foo"]
          })json",
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
