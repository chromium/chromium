// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_trigger_config.h"

#include <optional>
#include <string>

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_filtering_id_max_bytes.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationTimeConfig;
using ::attribution_reporting::mojom::TriggerRegistrationError;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::Optional;
using ::testing::Property;

const AggregatableFilteringIdsMaxBytes kFilteringIdMaxBytes;

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
                      kAggregatableSourceRegistrationTimeValueInvalid),
      },
      {
          "aggregatable_source_registration_time_invalid_value",
          R"json({"aggregatable_source_registration_time":"unknown"})json",
          ErrorIs(TriggerRegistrationError::
                      kAggregatableSourceRegistrationTimeValueInvalid),
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
          "trigger_context_id_disallowed",
          R"json({
            "aggregatable_source_registration_time":"include",
            "trigger_context_id":"123"
          })json",
          ErrorIs(TriggerRegistrationError::
                      kTriggerContextIdInvalidSourceRegistrationTimeConfig),
          ValueIs(*AggregatableTriggerConfig::Create(
              SourceRegistrationTimeConfig::kInclude,
              /*trigger_context_id=*/std::nullopt, kFilteringIdMaxBytes)),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    base::Value::Dict input = base::test::ParseJsonDict(test_case.json);
    EXPECT_THAT(AggregatableTriggerConfig::Parse(input),
                test_case.enabled_matches);
  }
}

TEST(AggregatableTriggerConfigTest,
     ParseAggregatableFilteringIdMaxByte_FilteringIdsEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAttributionReportingAggregatableFilteringIds);
  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<
        base::expected<AggregatableTriggerConfig, TriggerRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "aggregatable_filtering_id_max_bytes",
          R"json({"aggregatable_filtering_id_max_bytes": 3})json",
          ValueIs(Property(
              &AggregatableTriggerConfig::aggregatable_filtering_id_max_bytes,
              *AggregatableFilteringIdsMaxBytes::Create(3))),
      },
      {
          "aggregatable_filtering_id_max_bytes_wrong_type",
          R"json({"aggregatable_filtering_id_max_bytes": "3"})json",
          ErrorIs(TriggerRegistrationError::
                      kAggregatableFilteringIdMaxBytesInvalidValue),
      },
      {
          "aggregatable_filtering_id_max_bytes_disallowed",
          R"json({
            "aggregatable_source_registration_time": "include",
            "aggregatable_filtering_id_max_bytes": 3
          })json",
          ErrorIs(
              TriggerRegistrationError::
                  kAggregatableFilteringIdsMaxBytesInvalidSourceRegistrationTimeConfig),
      },
      {
          "aggregatable_filtering_id_default_max_bytes_allowed",
          R"json({
            "aggregatable_source_registration_time": "include",
            "aggregatable_filtering_id_max_bytes": 1
          })json",
          ValueIs(Property(
              &AggregatableTriggerConfig::aggregatable_filtering_id_max_bytes,
              AggregatableFilteringIdsMaxBytes())),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    base::Value::Dict input = base::test::ParseJsonDict(test_case.json);
    EXPECT_THAT(AggregatableTriggerConfig::Parse(input), test_case.matches);
  }
}

TEST(AggregatableTriggerConfigTest,
     ParseAggregatableFilteringIdMaxByte_FilteringIdsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAttributionReportingAggregatableFilteringIds);
  const struct {
    const char* desc;
    const char* json;
  } kTestCases[] = {
      {
          "aggregatable_filtering_id_max_bytes",
          R"json({"aggregatable_filtering_id_max_bytes": 3})json",
      },
      {
          "aggregatable_filtering_id_max_bytes_wrong_type",
          R"json({"aggregatable_filtering_id_max_bytes": "3"})json",
      },
      {
          "aggregatable_filtering_id_max_bytes_disallowed",
          R"json({
            "aggregatable_source_registration_time": "include",
            "aggregatable_filtering_id_max_bytes": 3
          })json",
      },
      {
          "aggregatable_filtering_id_default_max_bytes_allowed",
          R"json({
            "aggregatable_source_registration_time": "include",
            "aggregatable_filtering_id_max_bytes": 1
          })json",
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    base::Value::Dict input = base::test::ParseJsonDict(test_case.json);
    // It should never return an error and it should always set the value to the
    // default.
    EXPECT_THAT(
        AggregatableTriggerConfig::Parse(input),
        ValueIs(Property(
            &AggregatableTriggerConfig::aggregatable_filtering_id_max_bytes,
            AggregatableFilteringIdsMaxBytes())));
  }
}

TEST(AggregatableTriggerConfigTest, Create) {
  const struct {
    const char* desc;
    SourceRegistrationTimeConfig source_registration_time_config;
    std::optional<std::string> trigger_context_id;
    AggregatableFilteringIdsMaxBytes filtering_id_max_bytes;
    std::optional<AggregatableTriggerConfig> expected;
    std::optional<bool> should_cause_a_report_to_be_sent_unconditionally;
  } kTestCases[] = {
      {
          "valid_exclude_source_registration_time_with_trigger_context_id",
          SourceRegistrationTimeConfig::kExclude,
          "123",
          kFilteringIdMaxBytes,
          *AggregatableTriggerConfig::Create(
              SourceRegistrationTimeConfig::kExclude, "123",
              kFilteringIdMaxBytes),
          /*should_cause_a_report_to_be_sent_unconditionally=*/true,
      },
      {
          "valid_exclude_source_registration_time_without_trigger_context_id",
          SourceRegistrationTimeConfig::kExclude,
          /*trigger_context_id=*/std::nullopt,
          kFilteringIdMaxBytes,
          AggregatableTriggerConfig(),
          /*should_cause_a_report_to_be_sent_unconditionally=*/false,
      },
      {
          "valid_include_source_registration_time_without_trigger_context_id",
          SourceRegistrationTimeConfig::kInclude,
          /*trigger_context_id=*/std::nullopt,
          kFilteringIdMaxBytes,
          *AggregatableTriggerConfig::Create(
              SourceRegistrationTimeConfig::kInclude, std::nullopt,
              kFilteringIdMaxBytes),
          /*should_cause_a_report_to_be_sent_unconditionally=*/false,
      },
      {
          "trigger_context_id_too_long",
          SourceRegistrationTimeConfig::kExclude,
          std::string(65, 'a'),
          kFilteringIdMaxBytes,
          /*expected=*/std::nullopt,
          /*should_cause_a_report_to_be_sent_unconditionally=*/std::nullopt,
      },
      {
          "trigger_context_id_disallowed",
          SourceRegistrationTimeConfig::kInclude,
          "123",
          kFilteringIdMaxBytes,
          /*expected=*/std::nullopt,
          /*should_cause_a_report_to_be_sent_unconditionally=*/std::nullopt,
      },
      {
          "non_default_filtering_id_max_bytes_disallowed",
          SourceRegistrationTimeConfig::kInclude,
          /*trigger_context_id=*/std::nullopt,
          *AggregatableFilteringIdsMaxBytes::Create(2),
          /*expected=*/std::nullopt,
          /*should_cause_a_report_to_be_sent_unconditionally=*/std::nullopt,
      },
      {
          "valid_non_default_filtering_id_max_bytes",
          SourceRegistrationTimeConfig::kExclude,
          /*trigger_context_id=*/std::nullopt,
          *AggregatableFilteringIdsMaxBytes::Create(2),
          /*expected=*/
          *AggregatableTriggerConfig::Create(
              SourceRegistrationTimeConfig::kExclude, std::nullopt,
              *AggregatableFilteringIdsMaxBytes::Create(2)),
          /*should_cause_a_report_to_be_sent_unconditionally=*/true,
      },
      {
          "valid_non_default_filtering_id_max_bytes_and_triger_context_id",
          SourceRegistrationTimeConfig::kExclude,
          /*trigger_context_id=*/"123",
          *AggregatableFilteringIdsMaxBytes::Create(2),
          /*expected=*/
          *AggregatableTriggerConfig::Create(
              SourceRegistrationTimeConfig::kExclude, "123",
              *AggregatableFilteringIdsMaxBytes::Create(2)),
          /*should_cause_a_report_to_be_sent_unconditionally=*/true,
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    ASSERT_EQ(
        test_case.expected.has_value(),
        test_case.should_cause_a_report_to_be_sent_unconditionally.has_value());

    auto actual = AggregatableTriggerConfig::Create(
        test_case.source_registration_time_config, test_case.trigger_context_id,
        test_case.filtering_id_max_bytes);
    EXPECT_EQ(actual, test_case.expected);
    if (test_case.expected.has_value()) {
      EXPECT_EQ(actual->ShouldCauseAReportToBeSentUnconditionally(),
                test_case.should_cause_a_report_to_be_sent_unconditionally);
    }
  }
}

TEST(AggregatableTriggerConfigTest, Parse_TriggerContextIdLength) {
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

TEST(AggregatableTriggerConfigTest, Serialize_FilteringIdsEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAttributionReportingAggregatableFilteringIds);
  const struct {
    AggregatableTriggerConfig input;
    const char* expected_json;
  } kTestCases[] = {
      {
          AggregatableTriggerConfig(),
          R"json({
            "aggregatable_filtering_id_max_bytes": 1,
            "aggregatable_source_registration_time": "exclude"
          })json",
      },
      {
          *AggregatableTriggerConfig::Create(
              SourceRegistrationTimeConfig::kInclude,
              /*trigger_context_id=*/std::nullopt, kFilteringIdMaxBytes),
          R"json({
            "aggregatable_filtering_id_max_bytes": 1,
            "aggregatable_source_registration_time": "include"
          })json",
      },
      {
          *AggregatableTriggerConfig::Create(
              SourceRegistrationTimeConfig::kExclude,
              /*trigger_context_id=*/"123",
              *AggregatableFilteringIdsMaxBytes::Create(3u)),
          R"json({
            "aggregatable_source_registration_time":"exclude",
            "aggregatable_filtering_id_max_bytes": 3,
            "trigger_context_id": "123"
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

TEST(AggregatableTriggerConfigTest, Serialize_FilteringIdsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAttributionReportingAggregatableFilteringIds);
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
              /*trigger_context_id=*/std::nullopt, kFilteringIdMaxBytes),
          R"json({
            "aggregatable_source_registration_time":"include"
          })json",
      },
      {
          *AggregatableTriggerConfig::Create(
              SourceRegistrationTimeConfig::kExclude,
              /*trigger_context_id=*/"123", kFilteringIdMaxBytes),
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
