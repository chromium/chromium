// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_trigger_config.h"

#include <string>

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationTimeConfig;
using ::attribution_reporting::mojom::TriggerRegistrationError;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::Optional;
using ::testing::Property;

TEST(AggregatableTriggerConfigTest, ParseAggregatableSourceRegistrationTime) {
  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<
        base::expected<AggregatableTriggerConfig, TriggerRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "empty",
          R"json({})json",
          ValueIs(AggregatableTriggerConfig()),
      },
      {
          "aggregatable_source_registration_time_include",
          R"json({"aggregatable_source_registration_time":"include"})json",
          ValueIs(Property(
              &AggregatableTriggerConfig::source_registration_time_config,
              SourceRegistrationTimeConfig::kInclude)),
      },
      {
          "aggregatable_source_registration_time_exclude",
          R"json({"aggregatable_source_registration_time":"exclude"})json",
          ValueIs(Property(
              &AggregatableTriggerConfig::source_registration_time_config,
              SourceRegistrationTimeConfig::kExclude)),
      },
      {
          "aggregatable_source_registration_time_wrong_type",
          R"json({"aggregatable_source_registration_time":123})json",
          ErrorIs(TriggerRegistrationError::
                      kAggregatableSourceRegistrationTimeWrongType),
      },
      {
          "aggregatable_source_registration_time_invalid_value",
          R"json({"aggregatable_source_registration_time":"unknown"})json",
          ErrorIs(TriggerRegistrationError::
                      kAggregatableSourceRegistrationTimeUnknownValue),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    base::Value::Dict input = base::test::ParseJsonDict(test_case.json);

    EXPECT_THAT(AggregatableTriggerConfig::Parse(input), test_case.matches);
  }
}

TEST(AggregatableTriggerConfigTest, ParseTriggerContextId) {
  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<
        base::expected<AggregatableTriggerConfig, TriggerRegistrationError>>
        enabled_matches;
    ::testing::Matcher<
        base::expected<AggregatableTriggerConfig, TriggerRegistrationError>>
        disabled_matches;
  } kTestCases[] = {
      {
          "empty",
          R"json({})json",
          ValueIs(AggregatableTriggerConfig()),
          ValueIs(AggregatableTriggerConfig()),
      },
      {
          "trigger_context_id_valid",
          R"json({"trigger_context_id":"123"})json",
          ValueIs(Property(&AggregatableTriggerConfig::trigger_context_id,
                           Optional(std::string("123")))),
          ValueIs(AggregatableTriggerConfig()),
      },
      {
          "trigger_context_id_wrong_type",
          R"json({"trigger_context_id":123})json",
          ErrorIs(TriggerRegistrationError::kTriggerContextIdInvalidValue),
          ValueIs(AggregatableTriggerConfig()),
      },
      {
          "trigger_context_id_invalid_value",
          R"json({"trigger_context_id":""})json",
          ErrorIs(TriggerRegistrationError::kTriggerContextIdInvalidValue),
          ValueIs(AggregatableTriggerConfig()),
      },
      {
          "trigger_context_id_disallowed",
          R"json({
            "aggregatable_source_registration_time":"include",
            "trigger_context_id":"123"
          })json",
          ErrorIs(TriggerRegistrationError::
                      kTriggerContextIdInvalidSourceRegistrationTimeConfig),
          ValueIs(*AggregatableTriggerConfig::Create(
              SourceRegistrationTimeConfig::kInclude,
              /*trigger_context_id=*/absl::nullopt)),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    base::Value::Dict input = base::test::ParseJsonDict(test_case.json);

    {
      SCOPED_TRACE("disabled");

      base::test::ScopedFeatureList scoped_feature_list;
      scoped_feature_list.InitAndDisableFeature(
          features::kAttributionReportingTriggerContextId);

      EXPECT_THAT(AggregatableTriggerConfig::Parse(input),
                  test_case.disabled_matches);
    }

    {
      SCOPED_TRACE("enabled");

      base::test::ScopedFeatureList scoped_feature_list(
          features::kAttributionReportingTriggerContextId);

      EXPECT_THAT(AggregatableTriggerConfig::Parse(input),
                  test_case.enabled_matches);
    }
  }
}

TEST(AggregatableTriggerConfigTest, Create) {
  const struct {
    const char* desc;
    SourceRegistrationTimeConfig source_registration_time_config;
    absl::optional<std::string> trigger_context_id;
    absl::optional<AggregatableTriggerConfig> expected;
  } kTestCases[] = {
      {
          "valid_exclude_source_registration_time_with_trigger_context_id",
          SourceRegistrationTimeConfig::kExclude,
          "123",
          *AggregatableTriggerConfig::Create(
              SourceRegistrationTimeConfig::kExclude, "123"),
      },
      {
          "valid_exclude_source_registration_time_without_trigger_context_id",
          SourceRegistrationTimeConfig::kExclude,
          absl::nullopt,
          AggregatableTriggerConfig(),
      },
      {
          "valid_include_source_registration_time_without_trigger_context_id",
          SourceRegistrationTimeConfig::kInclude,
          absl::nullopt,
          *AggregatableTriggerConfig::Create(
              SourceRegistrationTimeConfig::kInclude, absl::nullopt),
      },
      {
          "trigger_context_id_empty",
          SourceRegistrationTimeConfig::kExclude,
          "",
          absl::nullopt,
      },
      {
          "trigger_context_id_too_long",
          SourceRegistrationTimeConfig::kExclude,
          std::string(65, 'a'),
          absl::nullopt,
      },
      {
          "trigger_context_id_disallowed",
          SourceRegistrationTimeConfig::kInclude,
          "123",
          absl::nullopt,
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    EXPECT_EQ(AggregatableTriggerConfig::Create(
                  test_case.source_registration_time_config,
                  test_case.trigger_context_id),
              test_case.expected);
  }
}

TEST(AggregatableTriggerConfigTest, Parse_TriggerContextIdLength) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAttributionReportingTriggerContextId);

  constexpr char kTriggerContextId[] = "trigger_context_id";

  base::Value::Dict dict;
  dict.Set(kTriggerContextId, std::string(64, 'a'));
  EXPECT_THAT(AggregatableTriggerConfig::Parse(dict),
              ValueIs(Property(&AggregatableTriggerConfig::trigger_context_id,
                               Optional(std::string(64, 'a')))));

  dict.Set(kTriggerContextId, std::string(65, 'a'));
  EXPECT_THAT(AggregatableTriggerConfig::Parse(dict),
              ErrorIs(TriggerRegistrationError::kTriggerContextIdInvalidValue));
}

TEST(AggregatableTriggerConfigTest, Serialize) {
  const struct {
    AggregatableTriggerConfig input;
    const char* expected_json;
  } kTestCases[] = {
      {
          AggregatableTriggerConfig(),
          R"json({
            "aggregatable_source_registration_time":"exclude"
          })json",
      },
      {
          *AggregatableTriggerConfig::Create(
              SourceRegistrationTimeConfig::kInclude,
              /*trigger_context_id=*/absl::nullopt),
          R"json({
            "aggregatable_source_registration_time":"include"
          })json",
      },
      {
          *AggregatableTriggerConfig::Create(
              SourceRegistrationTimeConfig::kExclude,
              /*trigger_context_id=*/"123"),
          R"json({
            "aggregatable_source_registration_time":"exclude",
            "trigger_context_id":"123"
          })json",
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.input);

    base::Value::Dict dict;
    test_case.input.Serialize(dict);
    EXPECT_THAT(dict, base::test::IsJson(test_case.expected_json));
  }
}

}  // namespace
}  // namespace attribution_reporting
