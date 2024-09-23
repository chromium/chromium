// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_debug_reporting_config.h"

#include <iterator>
#include <optional>

#include "base/strings/string_util.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "components/attribution_reporting/debug_types.mojom.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::DebugDataType;

using ::base::test::ErrorIs;
using ::base::test::HasValue;
using ::base::test::ValueIs;

using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Property;
using ::testing::SizeIs;

constexpr size_t kNumSourceDebugDataTypes = 15;
constexpr size_t kNumTriggerDebugDataTypes = 21;

TEST(AggregatableDebugReportingConfig, Parse) {
  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<base::expected<SourceAggregatableDebugReportingConfig,
                                      AggregatableDebugReportingConfigError>>
        matches_source;
    ::testing::Matcher<base::expected<AggregatableDebugReportingConfig,
                                      AggregatableDebugReportingConfigError>>
        matches_trigger;
    std::optional<AggregatableDebugReportingConfigError> expected_metric_source;
    std::optional<AggregatableDebugReportingConfigError>
        expected_metric_trigger;
  } kTestCases[] = {
      {
          .desc = "missing",
          .json = R"json({})json",
          .matches_source = ValueIs(SourceAggregatableDebugReportingConfig()),
          .matches_trigger = ValueIs(AggregatableDebugReportingConfig()),
      },
      {
          .desc = "wrong_type",
          .json = R"json({"aggregatable_debug_reporting": ""})json",
          .matches_source =
              ErrorIs(AggregatableDebugReportingConfigError::kRootInvalid),
          .matches_trigger =
              ErrorIs(AggregatableDebugReportingConfigError::kRootInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kRootInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kRootInvalid,
      },
      {
          .desc = "budget_missing",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "key_piece": "0x2"
            }
          })json",
          .matches_source =
              ErrorIs(AggregatableDebugReportingConfigError::kBudgetInvalid),
          .matches_trigger = HasValue(),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kBudgetInvalid,
      },
      {
          .desc = "budget_wrong_type",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": "1",
              "key_piece": "0x2"
            }
          })json",
          .matches_source =
              ErrorIs(AggregatableDebugReportingConfigError::kBudgetInvalid),
          .matches_trigger = HasValue(),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kBudgetInvalid,
      },
      {
          .desc = "budget_not_int",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 1.5,
              "key_piece": "0x2"
            }
          })json",
          .matches_source =
              ErrorIs(AggregatableDebugReportingConfigError::kBudgetInvalid),
          .matches_trigger = HasValue(),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kBudgetInvalid,
      },
      {
          .desc = "budget_less_than_min",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 0,
              "key_piece": "0x2"
            }
          })json",
          .matches_source =
              ErrorIs(AggregatableDebugReportingConfigError::kBudgetInvalid),
          .matches_trigger = HasValue(),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kBudgetInvalid,
      },
      {
          .desc = "budget_greater_than_max",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 65537,
              "key_piece": "0x2",
            }
          })json",
          .matches_source =
              ErrorIs(AggregatableDebugReportingConfigError::kBudgetInvalid),
          .matches_trigger = HasValue(),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kBudgetInvalid,
      },
      {
          .desc = "key_piece_missing",
          .json = R"json({"aggregatable_debug_reporting": {"budget": 1}})json",
          .matches_source =
              ErrorIs(AggregatableDebugReportingConfigError::kKeyPieceInvalid),
          .matches_trigger =
              ErrorIs(AggregatableDebugReportingConfigError::kKeyPieceInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kKeyPieceInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kKeyPieceInvalid,
      },
      {
          .desc = "key_piece_wrong_type",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 1,
              "key_piece": 1
            }
          })json",
          .matches_source =
              ErrorIs(AggregatableDebugReportingConfigError::kKeyPieceInvalid),
          .matches_trigger =
              ErrorIs(AggregatableDebugReportingConfigError::kKeyPieceInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kKeyPieceInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kKeyPieceInvalid,
      },
      {
          .desc = "key_piece_wrong_format",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 1,
              "key_piece": "1"
            }
          })json",
          .matches_source =
              ErrorIs(AggregatableDebugReportingConfigError::kKeyPieceInvalid),
          .matches_trigger =
              ErrorIs(AggregatableDebugReportingConfigError::kKeyPieceInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kKeyPieceInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kKeyPieceInvalid,
      },
      {
          .desc = "debug_data_wrong_type",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 1,
              "key_piece": "0x1",
              "debug_data": {}
            }
          })json",
          .matches_source =
              ErrorIs(AggregatableDebugReportingConfigError::kDebugDataInvalid),
          .matches_trigger =
              ErrorIs(AggregatableDebugReportingConfigError::kDebugDataInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kDebugDataInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kDebugDataInvalid,
      },
      {
          .desc = "debug_data_elem_wrong_type",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 1,
              "key_piece": "0x1",
              "debug_data": ["1"]
            }
          })json",
          .matches_source =
              ErrorIs(AggregatableDebugReportingConfigError::kDebugDataInvalid),
          .matches_trigger =
              ErrorIs(AggregatableDebugReportingConfigError::kDebugDataInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kDebugDataInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kDebugDataInvalid,
      },
      {
          .desc = "debug_data_elem_key_piece_missing",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 1,
              "key_piece": "0x1",
              "debug_data": [{}]
            }
          })json",
          .matches_source = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataKeyPieceInvalid),
          .matches_trigger = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataKeyPieceInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kDebugDataKeyPieceInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kDebugDataKeyPieceInvalid,
      },
      {
          .desc = "debug_data_elem_key_piece_wrong_type",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 1,
              "key_piece": "0x1",
              "debug_data": [{"key_piece": 1}]
            }
          })json",
          .matches_source = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataKeyPieceInvalid),
          .matches_trigger = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataKeyPieceInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kDebugDataKeyPieceInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kDebugDataKeyPieceInvalid,
      },
      {
          .desc = "debug_data_elem_key_piece_wrong_format",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 1,
              "key_piece": "0x1",
              "debug_data": [{"key_piece": "1"}]
            }
          })json",
          .matches_source = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataKeyPieceInvalid),
          .matches_trigger = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataKeyPieceInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kDebugDataKeyPieceInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kDebugDataKeyPieceInvalid,
      },
      {
          .desc = "debug_data_elem_value_wrong_type",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 1,
              "key_piece": "0x1",
              "debug_data": [{"key_piece": "0x1", "value": "1"}]
            }
          })json",
          .matches_source = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataValueInvalid),
          .matches_trigger = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataValueInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kDebugDataValueInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kDebugDataValueInvalid,
      },
      {
          .desc = "debug_data_elem_value_less_than_min",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 1,
              "key_piece": "0x1",
              "debug_data": [{"key_piece": "0x1", "value": 0}]
            }
          })json",
          .matches_source = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataValueInvalid),
          .matches_trigger = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataValueInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kDebugDataValueInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kDebugDataValueInvalid,
      },
      {
          .desc = "debug_data_elem_value_not_int",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 2,
              "key_piece": "0x1",
              "debug_data": [{"key_piece": "0x1", "value": 1.5}]
            }
          })json",
          .matches_source = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataValueInvalid),
          .matches_trigger = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataValueInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kDebugDataValueInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kDebugDataValueInvalid,
      },
      {
          .desc = "debug_data_elem_value_greater_than_max",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 65536,
              "key_piece": "0x1",
              "debug_data": [{"key_piece": "0x1", "value": 65537}]
            }
          })json",
          .matches_source = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataValueInvalid),
          .matches_trigger = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataValueInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kDebugDataValueInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kDebugDataValueInvalid,
      },
      {
          .desc = "debug_data_elem_value_greater_than_budget",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 10,
              "key_piece": "0x1",
              "debug_data": [{
                "key_piece": "0x1",
                "value": 11,
                "types": ["abc"]
              }]
            }
          })json",
          .matches_source = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataValueInvalid),
          .matches_trigger = HasValue(),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kDebugDataValueInvalid,
      },
      {
          .desc = "debug_data_elem_types_missing",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 10,
              "key_piece": "0x1",
              "debug_data": [{"key_piece": "0x1", "value": 10}]
            }
          })json",
          .matches_source = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid),
          .matches_trigger = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid,
      },
      {
          .desc = "debug_data_elem_types_wrong_type",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 10,
              "key_piece": "0x1",
              "debug_data": [{"key_piece": "0x1", "value": 10, "types": {}}]
            }
          })json",
          .matches_source = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid),
          .matches_trigger = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid,
      },
      {
          .desc = "debug_data_elem_types_empty",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 10,
              "key_piece": "0x1",
              "debug_data": [{"key_piece": "0x1", "value": 10, "types": []}]
            }
          })json",
          .matches_source = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid),
          .matches_trigger = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid,
      },
      {
          .desc = "debug_data_elem_types_elem_wrong_type",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 10,
              "key_piece": "0x1",
              "debug_data": [{"key_piece": "0x1", "value": 10, "types": [1]}]
            }
          })json",
          .matches_source = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid),
          .matches_trigger = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid,
      },
      {
          .desc = "debug_data_elem_types_elem_duplicate",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 10,
              "key_piece": "0x1",
              "debug_data": [{
                "key_piece": "0x1",
                "value": 10,
                "types": ["unspecified", "unspecified"]
              }]
            }
          })json",
          .matches_source = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid),
          .matches_trigger = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid,
      },
      {
          .desc = "debug_data_elem_types_elem_duplicate_across",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 10,
              "key_piece": "0x1",
              "debug_data": [
                {"key_piece": "0x1", "value": 10, "types": ["unspecified"]},
                {"key_piece": "0x1", "value": 10, "types": ["unspecified"]}
              ]
            }
          })json",
          .matches_source = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid),
          .matches_trigger = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid,
      },
      {
          .desc = "debug_data_elem_types_elem_unknown_duplicate",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 10,
              "key_piece": "0x1",
              "debug_data": [
                {"key_piece": "0x1", "value": 10, "types": ["a", "a"]}
              ]
            }
          })json",
          .matches_source = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid),
          .matches_trigger = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid,
      },
      {
          .desc = "debug_data_elem_types_elem_unknown_duplicate_across",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 10,
              "key_piece": "0x1",
              "debug_data": [
                {"key_piece": "0x1", "value": 10, "types": ["a"]},
                {"key_piece": "0x1", "value": 10, "types": ["a"]}
              ]
            }
          })json",
          .matches_source = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid),
          .matches_trigger = ErrorIs(
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid),
          .expected_metric_source =
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid,
          .expected_metric_trigger =
              AggregatableDebugReportingConfigError::kDebugDataTypesInvalid,
      },
      {
          .desc = "debug_data_aggregation_coordinator_origin_wrong_type",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 10,
              "key_piece": "0x1",
              "aggregation_coordinator_origin": 1
            }
          })json",
          .matches_source = ErrorIs(AggregatableDebugReportingConfigError::
                                        kAggregationCoordinatorOriginInvalid),
          .matches_trigger = ErrorIs(AggregatableDebugReportingConfigError::
                                         kAggregationCoordinatorOriginInvalid),
          .expected_metric_source = AggregatableDebugReportingConfigError::
              kAggregationCoordinatorOriginInvalid,
          .expected_metric_trigger = AggregatableDebugReportingConfigError::
              kAggregationCoordinatorOriginInvalid,
      },
      {
          .desc = "debug_data_aggregation_coordinator_origin_invalid",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 10,
              "key_piece": "0x1",
              "aggregation_coordinator_origin": "https://b.test",
            }
          })json",
          .matches_source = ErrorIs(AggregatableDebugReportingConfigError::
                                        kAggregationCoordinatorOriginInvalid),
          .matches_trigger = ErrorIs(AggregatableDebugReportingConfigError::
                                         kAggregationCoordinatorOriginInvalid),
          .expected_metric_source = AggregatableDebugReportingConfigError::
              kAggregationCoordinatorOriginInvalid,
          .expected_metric_trigger = AggregatableDebugReportingConfigError::
              kAggregationCoordinatorOriginInvalid,
      },
      {
          .desc = "all_fields",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 10,
              "key_piece": "0xf",
              "debug_data": [
                {
                  "key_piece": "0x2",
                  "value": 3,
                  "types": ["source-success"]
                },
                {
                  "key_piece": "0xA",
                  "value": 9,
                  "types": ["trigger-no-matching-source"]
                },
                {
                  "key_piece": "0x1",
                  "value": 2,
                  "types": ["unspecified"]
                }
              ],
              "aggregation_coordinator_origin": "https://a.test"
            }
          })json",
          .matches_source = ValueIs(AllOf(
              Property(&SourceAggregatableDebugReportingConfig::budget, 10),
              Property(
                  &SourceAggregatableDebugReportingConfig::config,
                  AllOf(
                      Field(&AggregatableDebugReportingConfig::key_piece, 15),
                      Field(&AggregatableDebugReportingConfig::
                                aggregation_coordinator_origin,
                            *SuitableOrigin::Create(GURL("https://a.test"))),
                      Field(&AggregatableDebugReportingConfig::debug_data,
                            AllOf(SizeIs(kNumSourceDebugDataTypes),
                                  testing::Contains(Pair(
                                      DebugDataType::kSourceSuccess,
                                      *AggregatableDebugReportingContribution::
                                          Create(2, 3))))))))),
          .matches_trigger = ValueIs(AllOf(
              Field(&AggregatableDebugReportingConfig::key_piece, 15),
              Field(&AggregatableDebugReportingConfig::
                        aggregation_coordinator_origin,
                    *SuitableOrigin::Create(GURL("https://a.test"))),
              Field(&AggregatableDebugReportingConfig::debug_data,
                    AllOf(SizeIs(kNumTriggerDebugDataTypes),
                          testing::Contains(Pair(
                              DebugDataType::kTriggerNoMatchingSource,
                              *AggregatableDebugReportingContribution::Create(
                                  10, 9))))))),
      },
      {
          .desc = "all_fields_trailing_zero",
          .json = R"json({
            "aggregatable_debug_reporting": {
              "budget": 10.0,
              "key_piece": "0xf",
              "debug_data": [
                {
                  "key_piece": "0x2",
                  "value": 3.0,
                  "types": ["unspecified"]
                }
              ],
              "aggregation_coordinator_origin": "https://a.test"
            }
          })json",
          .matches_source = ValueIs(AllOf(
              Property(&SourceAggregatableDebugReportingConfig::budget, 10),
              Property(
                  &SourceAggregatableDebugReportingConfig::config,
                  Field(&AggregatableDebugReportingConfig::debug_data,
                        testing::Contains(Pair(
                            _, *AggregatableDebugReportingContribution::Create(
                                   2, 3))))))),
          .matches_trigger = ValueIs(Field(
              &AggregatableDebugReportingConfig::debug_data,
              testing::Contains(Pair(
                  _, *AggregatableDebugReportingContribution::Create(2, 3))))),
      },
  };

  aggregation_service::ScopedAggregationCoordinatorAllowlistForTesting
      scoped_coordinator_allowlist(
          {url::Origin::Create(GURL("https://a.test"))});

  base::test::ScopedFeatureList scoped_feature_list(
      features::kAttributionAggregatableDebugReporting);

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    {
      base::HistogramTester histograms;

      base::Value::Dict dict = base::test::ParseJsonDict(test_case.json);
      EXPECT_THAT(SourceAggregatableDebugReportingConfig::Parse(dict),
                  test_case.matches_source);

      if (auto expected_metric_source = test_case.expected_metric_source;
          expected_metric_source.has_value()) {
        histograms.ExpectUniqueSample(
            "Conversions.AggregatableDebugReporting.SourceRegistrationError",
            *expected_metric_source, 1);
      }
    }

    {
      base::HistogramTester histograms;

      base::Value::Dict dict = base::test::ParseJsonDict(test_case.json);
      EXPECT_THAT(AggregatableDebugReportingConfig::Parse(dict),
                  test_case.matches_trigger);

      if (auto expected_metric_trigger = test_case.expected_metric_trigger;
          expected_metric_trigger.has_value()) {
        histograms.ExpectUniqueSample(
            "Conversions.AggregatableDebugReporting.TriggerRegistrationError",
            *expected_metric_trigger, 1);
      }
    }
  }
}

TEST(AggregatableDebugReportingConfig, SerializeSource) {
  const struct {
    SourceAggregatableDebugReportingConfig input;
    const char* expected_json;
  } kTestCases[] = {
      {
          SourceAggregatableDebugReportingConfig(),
          R"json({})json",
      },
      {
          *SourceAggregatableDebugReportingConfig::Create(
              /*budget=*/123,
              AggregatableDebugReportingConfig(
                  /*key_piece=*/159,
                  /*debug_data=*/
                  {{DebugDataType::kSourceDestinationLimit,
                    *AggregatableDebugReportingContribution::Create(
                        /*key_piece=*/10, /*value=*/12)}},
                  /*aggregation_coordinator_origin=*/
                  *SuitableOrigin::Deserialize("https://a.test"))),
          R"json({
            "aggregatable_debug_reporting": {
              "budget": 123,
              "key_piece": "0x9f",
              "debug_data": [
                {
                  "key_piece": "0xa",
                  "types": ["source-destination-limit"],
                  "value": 12
                }
              ],
              "aggregation_coordinator_origin": "https://a.test"
            }
          })json",
      },
  };

  aggregation_service::ScopedAggregationCoordinatorAllowlistForTesting
      scoped_coordinator_allowlist(
          {url::Origin::Create(GURL("https://a.test"))});

  base::test::ScopedFeatureList scoped_feature_list(
      features::kAttributionAggregatableDebugReporting);

  for (const auto& test_case : kTestCases) {
    base::Value::Dict dict;
    test_case.input.Serialize(dict);
    EXPECT_THAT(dict, base::test::IsJson(test_case.expected_json));
  }
}

TEST(AggregatableDebugReportingConfig, SerializeTrigger) {
  const struct {
    AggregatableDebugReportingConfig input;
    const char* expected_json;
  } kTestCases[] = {
      {
          AggregatableDebugReportingConfig(),
          R"json({
            "aggregatable_debug_reporting": {
              "key_piece": "0x0"
            }
          })json",
      },
      {
          AggregatableDebugReportingConfig(
              /*key_piece=*/159,
              /*debug_data=*/
              {{DebugDataType::kTriggerNoMatchingSource,
                *AggregatableDebugReportingContribution::Create(
                    /*key_piece=*/10, /*value=*/12)}},
              /*aggregation_coordinator_origin=*/
              *SuitableOrigin::Deserialize("https://a.test")),
          R"json({
            "aggregatable_debug_reporting": {
              "key_piece": "0x9f",
              "debug_data": [
                {
                  "key_piece": "0xa",
                  "types": ["trigger-no-matching-source"],
                  "value": 12
                }
              ],
              "aggregation_coordinator_origin": "https://a.test"
            }
          })json",
      },
  };

  aggregation_service::ScopedAggregationCoordinatorAllowlistForTesting
      scoped_coordinator_allowlist(
          {url::Origin::Create(GURL("https://a.test"))});

  base::test::ScopedFeatureList scoped_feature_list(
      features::kAttributionAggregatableDebugReporting);

  for (const auto& test_case : kTestCases) {
    base::Value::Dict dict;
    test_case.input.Serialize(dict);
    EXPECT_THAT(dict, base::test::IsJson(test_case.expected_json));
  }
}

TEST(AggregatableDebugReportingConfig, SourceDebugDataTypes) {
  const struct {
    const char* type_str;
    DebugDataType expected_type;
  } kTestCases[] = {
      {
          "source-channel-capacity-limit",
          DebugDataType::kSourceChannelCapacityLimit,
      },
      {
          "source-destination-global-rate-limit",
          DebugDataType::kSourceDestinationGlobalRateLimit,
      },
      {
          "source-destination-limit",
          DebugDataType::kSourceDestinationLimit,
      },
      {
          "source-destination-limit-replaced",
          DebugDataType::kSourceDestinationLimitReplaced,
      },
      {
          "source-destination-per-day-rate-limit",
          DebugDataType::kSourceDestinationPerDayRateLimit,
      },
      {
          "source-destination-rate-limit",
          DebugDataType::kSourceDestinationRateLimit,
      },
      {
          "source-noised",
          DebugDataType::kSourceNoised,
      },
      {
          "source-reporting-origin-limit",
          DebugDataType::kSourceReportingOriginLimit,
      },
      {
          "source-reporting-origin-per-site-limit",
          DebugDataType::kSourceReportingOriginPerSiteLimit,
      },
      {
          "source-scopes-channel-capacity-limit",
          DebugDataType::kSourceScopesChannelCapacityLimit,
      },
      {
          "source-storage-limit",
          DebugDataType::kSourceStorageLimit,
      },
      {
          "source-success",
          DebugDataType::kSourceSuccess,
      },
      {
          "source-trigger-state-cardinality-limit",
          DebugDataType::kSourceTriggerStateCardinalityLimit,
      },
      {
          "source-max-event-states-limit",
          DebugDataType::kSourceMaxEventStatesLimit,
      },
      {
          "source-unknown-error",
          DebugDataType::kSourceUnknownError,
      },
  };

  static_assert(std::size(kTestCases) == kNumSourceDebugDataTypes);

  const char* json = R"json({
    "aggregatable_debug_reporting": {
      "budget": 10,
      "key_piece": "0xf",
      "debug_data": [{
        "key_piece": "0x2",
        "value": 3,
        "types": ["$1"]
      }]
    }
  })json";

  base::test::ScopedFeatureList scoped_feature_list(
      features::kAttributionAggregatableDebugReporting);

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.type_str);

    base::Value::Dict dict =
        base::test::ParseJsonDict(base::ReplaceStringPlaceholders(
            json, {test_case.type_str}, /*offsets=*/nullptr));
    EXPECT_THAT(SourceAggregatableDebugReportingConfig::Parse(dict),
                ValueIs(Property(
                    &SourceAggregatableDebugReportingConfig::config,
                    Field(&AggregatableDebugReportingConfig::debug_data,
                          ElementsAre(Pair(test_case.expected_type, _))))));
  }
}

TEST(AggregatableDebugReportingConfig, TriggerDebugDataTypes) {
  const struct {
    const char* type_str;
    DebugDataType expected_type;
  } kTestCases[] = {
      {
          "trigger-aggregate-attributions-per-source-destination-limit",
          DebugDataType::kTriggerAggregateAttributionsPerSourceDestinationLimit,
      },
      {
          "trigger-aggregate-deduplicated",
          DebugDataType::kTriggerAggregateDeduplicated,
      },
      {
          "trigger-aggregate-excessive-reports",
          DebugDataType::kTriggerAggregateExcessiveReports,
      },
      {
          "trigger-aggregate-insufficient-budget",
          DebugDataType::kTriggerAggregateInsufficientBudget,
      },
      {
          "trigger-aggregate-no-contributions",
          DebugDataType::kTriggerAggregateNoContributions,
      },
      {
          "trigger-aggregate-report-window-passed",
          DebugDataType::kTriggerAggregateReportWindowPassed,
      },
      {
          "trigger-aggregate-storage-limit",
          DebugDataType::kTriggerAggregateStorageLimit,
      },
      {
          "trigger-event-attributions-per-source-destination-limit",
          DebugDataType::kTriggerEventAttributionsPerSourceDestinationLimit,
      },
      {
          "trigger-event-deduplicated",
          DebugDataType::kTriggerEventDeduplicated,
      },
      {
          "trigger-event-excessive-reports",
          DebugDataType::kTriggerEventExcessiveReports,
      },
      {
          "trigger-event-low-priority",
          DebugDataType::kTriggerEventLowPriority,
      },
      {
          "trigger-event-no-matching-configurations",
          DebugDataType::kTriggerEventNoMatchingConfigurations,
      },
      {
          "trigger-event-no-matching-trigger-data",
          DebugDataType::kTriggerEventNoMatchingTriggerData,
      },
      {
          "trigger-event-noise",
          DebugDataType::kTriggerEventNoise,
      },
      {
          "trigger-event-report-window-not-started",
          DebugDataType::kTriggerEventReportWindowNotStarted,
      },
      {
          "trigger-event-report-window-passed",
          DebugDataType::kTriggerEventReportWindowPassed,
      },
      {
          "trigger-event-storage-limit",
          DebugDataType::kTriggerEventStorageLimit,
      },
      {
          "trigger-no-matching-filter-data",
          DebugDataType::kTriggerNoMatchingFilterData,
      },
      {
          "trigger-no-matching-source",
          DebugDataType::kTriggerNoMatchingSource,
      },
      {
          "trigger-reporting-origin-limit",
          DebugDataType::kTriggerReportingOriginLimit,
      },
      {
          "trigger-unknown-error",
          DebugDataType::kTriggerUnknownError,
      },
  };

  static_assert(std::size(kTestCases) == kNumTriggerDebugDataTypes);

  const char* json = R"json({
    "aggregatable_debug_reporting": {
      "key_piece": "0xf",
      "debug_data": [{
        "key_piece": "0x2",
        "value": 3,
        "types": ["$1"]
      }]
    }
  })json";

  base::test::ScopedFeatureList scoped_feature_list(
      features::kAttributionAggregatableDebugReporting);

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.type_str);

    base::Value::Dict dict =
        base::test::ParseJsonDict(base::ReplaceStringPlaceholders(
            json, {test_case.type_str}, /*offsets=*/nullptr));
    EXPECT_THAT(AggregatableDebugReportingConfig::Parse(dict),
                ValueIs(Field(&AggregatableDebugReportingConfig::debug_data,
                              ElementsAre(Pair(test_case.expected_type, _)))));
  }
}

}  // namespace
}  // namespace attribution_reporting
