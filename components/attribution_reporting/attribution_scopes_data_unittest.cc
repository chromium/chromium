// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/attribution_scopes_data.h"

#include <optional>

#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/attribution_scopes_set.h"
#include "components/attribution_reporting/constants.h"
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
          "wrong_type",
          R"json([])json",
          ErrorIs(SourceRegistrationError::kAttributionScopesInvalid),
      },
      {
          "empty",
          R"json({})json",
          ErrorIs(SourceRegistrationError::kAttributionScopeLimitRequired),
      },
      {
          "scope_limit_only",
          R"json({"limit": 1})json",
          ErrorIs(SourceRegistrationError::kAttributionScopesListInvalid),
      },
      {
          "scope_limit_wrong_type",
          R"json({
            "limit": "1"
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopeLimitInvalid),
      },
      {
          "scope_limit_zero",
          R"json({
            "limit": 0
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopeLimitInvalid),
      },
      {
          "scope_limit_double",
          R"json({
            "limit": 1.5
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopeLimitInvalid),
      },
      {
          "scope_limit_negative",
          R"json({
            "limit": -1
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopeLimitInvalid),
      },
      {
          "scope_limit_uint32_max",
          R"json({
            "limit": 4294967295,
            "values": ["1"]
          })json",
          ValueIs(*AttributionScopesData::Create(
              AttributionScopesSet({"1"}),
              /*attribution_scope_limit=*/std::numeric_limits<uint32_t>::max(),
              /*max_event_states=*/kDefaultMaxEventStates)),
      },
      {
          "scope_limit_greater_than_uint32",
          R"json({
            "limit": 4294967296
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopeLimitInvalid),
      },
      {
          "scopes_wrong_type",
          R"json({
            "limit": 1,
            "values": "1"
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopesListInvalid),
      },
      {
          "scope_limit_and_scopes",
          R"json({
            "limit": 1,
            "values": ["1"]
          })json",
          ValueIs(*AttributionScopesData::Create(AttributionScopesSet({"1"}),
                                                 /*attribution_scope_limit=*/1u,
                                                 kDefaultMaxEventStates)),
      },
      {
          "empty_scopes",
          R"json({
            "limit": 1,
            "values": []
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopesListInvalid),
      },
      {
          "scopes_exceed_limit",
          R"json({
            "limit": 1,
            "values": ["1", "2"]
          })json",
          ErrorIs(SourceRegistrationError::kAttributionScopesListInvalid),
      },
      {
          "event_states_wrong_type",
          R"json({
            "limit": 1,
            "max_event_states": "1"
          })json",
          ErrorIs(SourceRegistrationError::kMaxEventStatesInvalid),
      },
      {
          "event_states_zero",
          R"json({
            "limit": 1,
            "max_event_states": 0
          })json",
          ErrorIs(SourceRegistrationError::kMaxEventStatesInvalid),
      },
      {
          "event_states_double",
          R"json({
            "limit": 1,
            "max_event_states": 1.5
          })json",
          ErrorIs(SourceRegistrationError::kMaxEventStatesInvalid),
      },
      {
          "event_states_negative",
          R"json({
            "limit": 1,
            "max_event_states": -1
          })json",
          ErrorIs(SourceRegistrationError::kMaxEventStatesInvalid),
      },
      {
          "event_states_uint32_max",
          R"json({
            "limit": 1,
            "max_event_states": 4294967295,
            "values": ["1"]
          })json",
          ValueIs(*AttributionScopesData::Create(
              AttributionScopesSet({"1"}), /*attribution_scope_limit=*/1u,
              /*max_event_states=*/std::numeric_limits<uint32_t>::max())),
      },
      {
          "event_states_greater_than_uint32",
          R"json({
            "limit": 1,
            "max_event_states": 4294967296
          })json",
          ErrorIs(SourceRegistrationError::kMaxEventStatesInvalid),
      },
      {
          "all_fields",
          R"json({
            "limit": 1,
            "max_event_states": 1,
            "values": ["1"]
          })json",
          ValueIs(*AttributionScopesData::Create(AttributionScopesSet({"1"}),
                                                 /*attribution_scope_limit=*/1u,
                                                 /*max_event_states=*/1u)),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    base::Value value = base::test::ParseJson(test_case.json);
    EXPECT_THAT(AttributionScopesData::FromJSON(value), test_case.matches);
  }
}

TEST(AttributionScopesDataTest, Serialize) {
  const struct {
    AttributionScopesData input;
    const char* expected_json;
  } kTestCases[] = {
      {
          *AttributionScopesData::Create(AttributionScopesSet({"foo"}),
                                         /*attribution_scope_limit=*/1u,
                                         /*max_event_states=*/1u),
          R"json({
            "limit": 1,
            "max_event_states": 1,
            "values": ["foo"]
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
