// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_config.h"

#include <utility>

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_data_matching.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SourceType;
using ::attribution_reporting::mojom::TriggerDataMatching;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Key;
using ::testing::Pair;
using ::testing::Property;
using ::testing::SizeIs;

TEST(TriggerConfigTest, Parse) {
  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<base::expected<TriggerConfig, SourceRegistrationError>>
        disabled_matches;
    ::testing::Matcher<base::expected<TriggerConfig, SourceRegistrationError>>
        enabled_matches;
  } kTestCases[] = {
      {
          .desc = "missing",
          .json = R"json({})json",
          .disabled_matches =
              ValueIs(Property(&TriggerConfig::trigger_data_matching,
                               TriggerDataMatching::kModulus)),
          .enabled_matches =
              ValueIs(Property(&TriggerConfig::trigger_data_matching,
                               TriggerDataMatching::kModulus)),
      },
      {
          .desc = "wrong_type",
          .json = R"json({"trigger_data_matching":1})json",
          .disabled_matches =
              ValueIs(Property(&TriggerConfig::trigger_data_matching,
                               TriggerDataMatching::kModulus)),
          .enabled_matches =
              ErrorIs(SourceRegistrationError::kTriggerDataMatchingWrongType),
      },
      {
          .desc = "invalid_value",
          .json = R"json({"trigger_data_matching":"MODULUS"})json",
          .disabled_matches =
              ValueIs(Property(&TriggerConfig::trigger_data_matching,
                               TriggerDataMatching::kModulus)),
          .enabled_matches = ErrorIs(
              SourceRegistrationError::kTriggerDataMatchingUnknownValue),
      },
      {
          .desc = "valid_modulus",
          .json = R"json({"trigger_data_matching":"modulus"})json",
          .disabled_matches =
              ValueIs(Property(&TriggerConfig::trigger_data_matching,
                               TriggerDataMatching::kModulus)),
          .enabled_matches =
              ValueIs(Property(&TriggerConfig::trigger_data_matching,
                               TriggerDataMatching::kModulus)),
      },
      {
          .desc = "valid_exact",
          .json = R"json({"trigger_data_matching":"exact"})json",
          .disabled_matches =
              ValueIs(Property(&TriggerConfig::trigger_data_matching,
                               TriggerDataMatching::kModulus)),
          .enabled_matches =
              ValueIs(Property(&TriggerConfig::trigger_data_matching,
                               TriggerDataMatching::kExact)),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    const base::Value::Dict dict = base::test::ParseJsonDict(test_case.json);

    {
      SCOPED_TRACE("disabled");
      EXPECT_THAT(TriggerConfig::Parse(dict), test_case.disabled_matches);
    }

    {
      SCOPED_TRACE("enabled");

      base::test::ScopedFeatureList scoped_feature_list(
          features::kAttributionReportingTriggerConfig);

      EXPECT_THAT(TriggerConfig::Parse(dict), test_case.enabled_matches);
    }
  }
}

TEST(TriggerConfigTest, Serialize) {
  const struct {
    TriggerDataMatching trigger_data_matching;
    base::Value::Dict disabled_expected;
    base::Value::Dict enabled_expected;
  } kTestCases[] = {
      {
          TriggerDataMatching::kModulus,
          base::Value::Dict(),
          base::Value::Dict().Set("trigger_data_matching", "modulus"),
      },
      {
          TriggerDataMatching::kExact,
          base::Value::Dict(),
          base::Value::Dict().Set("trigger_data_matching", "exact"),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.trigger_data_matching);

    {
      SCOPED_TRACE("disabled");

      base::Value::Dict dict;
      TriggerConfig(test_case.trigger_data_matching).Serialize(dict);
      EXPECT_EQ(dict, test_case.disabled_expected);
    }

    {
      SCOPED_TRACE("enabled");

      base::test::ScopedFeatureList scoped_feature_list(
          features::kAttributionReportingTriggerConfig);

      base::Value::Dict dict;
      TriggerConfig(test_case.trigger_data_matching).Serialize(dict);
      EXPECT_EQ(dict, test_case.enabled_expected);
    }
  }
}

TEST(TriggerSpecsTest, Default) {
  const auto kReportWindows = *EventReportWindows::Create(
      /*start_time=*/base::Hours(2),
      /*end_times=*/{base::Hours(9)});

  EXPECT_THAT(TriggerSpecs::Default(SourceType::kEvent, kReportWindows),
              ElementsAre(Pair(0, TriggerSpec(kReportWindows)),
                          Pair(1, TriggerSpec(kReportWindows))));

  EXPECT_THAT(TriggerSpecs::Default(SourceType::kNavigation, kReportWindows),
              ElementsAre(Pair(0, TriggerSpec(kReportWindows)),
                          Pair(1, TriggerSpec(kReportWindows)),
                          Pair(2, TriggerSpec(kReportWindows)),
                          Pair(3, TriggerSpec(kReportWindows)),
                          Pair(4, TriggerSpec(kReportWindows)),
                          Pair(5, TriggerSpec(kReportWindows)),
                          Pair(6, TriggerSpec(kReportWindows)),
                          Pair(7, TriggerSpec(kReportWindows))));
}

TEST(TriggerSpecsTest, Parse) {
  constexpr base::TimeDelta kExpiry = base::Days(30);
  const EventReportWindows kDefaultReportWindows;

  const struct {
    const char* desc;
    const char* json;
    SourceType source_type = SourceType::kNavigation;
    TriggerDataMatching trigger_data_matching = TriggerDataMatching::kExact;
    // Expected output when the feature is enabled.
    ::testing::Matcher<base::expected<TriggerSpecs, SourceRegistrationError>>
        enabled_matches;
  } kTestCases[] = {
      {
          .desc = "missing_navigation",
          .json = R"json({})json",
          .source_type = SourceType::kNavigation,
          .enabled_matches = ValueIs(TriggerSpecs::Default(
              SourceType::kNavigation, kDefaultReportWindows)),
      },
      {
          .desc = "missing_event",
          .json = R"json({})json",
          .source_type = SourceType::kEvent,
          .enabled_matches = ValueIs(
              TriggerSpecs::Default(SourceType::kEvent, kDefaultReportWindows)),
      },
      {
          .desc = "wrong_type",
          .json = R"json({"trigger_specs": 1})json",
          .enabled_matches =
              ErrorIs(SourceRegistrationError::kTriggerSpecsWrongType),
      },
      {
          .desc = "too_long",
          .json = R"json({"trigger_specs": [
             0,  1,  2,  3,  4,  5,  6,  7,
             8,  9, 10, 11, 12, 13, 14, 15,
            16, 17, 18, 19, 20, 21, 22, 23,
            24, 25, 26, 27, 28, 29, 30, 31,
            32
          ]})json",
          .enabled_matches =
              ErrorIs(SourceRegistrationError::kExcessiveTriggerData),
      },
      {
          .desc = "spec_wrong_type",
          .json = R"json({"trigger_specs": [0]})json",
          .enabled_matches =
              ErrorIs(SourceRegistrationError::kTriggerSpecWrongType),
      },
      {
          .desc = "trigger_data_missing",
          .json = R"json({"trigger_specs": [{}]})json",
          .enabled_matches =
              ErrorIs(SourceRegistrationError::kTriggerSpecTriggerDataMissing),
      },
      {
          .desc = "trigger_data_wrong_type",
          .json = R"json({"trigger_specs": [{"trigger_data": 1}]})json",
          .enabled_matches = ErrorIs(
              SourceRegistrationError::kTriggerSpecTriggerDataWrongType),
      },
      {
          .desc = "trigger_data_empty",
          .json = R"json({"trigger_specs": [{"trigger_data": []}]})json",
          .enabled_matches =
              ErrorIs(SourceRegistrationError::kTriggerSpecTriggerDataEmpty),
      },
      {
          .desc = "trigger_data_too_long",
          .json = R"json({"trigger_specs": [{"trigger_data": [
             0,  1,  2,  3,  4,  5,  6,  7,
             8,  9, 10, 11, 12, 13, 14, 15,
            16, 17, 18, 19, 20, 21, 22, 23,
            24, 25, 26, 27, 28, 29, 30, 31,
            32
          ]}]})json",
          .enabled_matches =
              ErrorIs(SourceRegistrationError::kExcessiveTriggerData),
      },
      {
          .desc = "trigger_data_value_wrong_type",
          .json = R"json({"trigger_specs": [{"trigger_data": ["1"]}]})json",
          .enabled_matches = ErrorIs(
              SourceRegistrationError::kTriggerSpecTriggerDataValueWrongType),
      },
      {
          .desc = "trigger_data_value_fractional",
          .json = R"json({"trigger_specs": [{"trigger_data": [1.5]}]})json",
          .enabled_matches = ErrorIs(
              SourceRegistrationError::kTriggerSpecTriggerDataValueWrongType),
      },
      {
          .desc = "trigger_data_value_negative",
          .json = R"json({"trigger_specs": [{"trigger_data": [-1]}]})json",
          .enabled_matches = ErrorIs(
              SourceRegistrationError::kTriggerSpecTriggerDataValueOutOfRange),
      },
      {
          .desc = "trigger_data_value_above_max",
          .json =
              R"json({"trigger_specs": [{"trigger_data": [4294967296]}]})json",
          .enabled_matches = ErrorIs(
              SourceRegistrationError::kTriggerSpecTriggerDataValueOutOfRange),
      },
      {
          .desc = "trigger_data_value_minimal",
          .json = R"json({"trigger_specs": [{"trigger_data": [0]}]})json",
          .enabled_matches = ValueIs(ElementsAre(Key(0))),
      },
      {
          .desc = "trigger_data_value_maximal",
          .json =
              R"json({"trigger_specs": [{"trigger_data": [4294967295]}]})json",
          .enabled_matches = ValueIs(ElementsAre(Key(4294967295))),
      },
      {
          .desc = "trigger_data_value_duplicate_within_spec",
          .json =
              R"json({"trigger_specs": [{"trigger_data": [1, 3, 1, 2]}]})json",
          .enabled_matches =
              ErrorIs(SourceRegistrationError::kDuplicateTriggerData),
      },
      {
          .desc = "trigger_data_value_duplicate_across_specs",
          .json = R"json({"trigger_specs": [
            {"trigger_data": [1, 3]},
            {"trigger_data": [4, 2]},
            {"trigger_data": [1, 5]},
          ]})json",
          .enabled_matches =
              ErrorIs(SourceRegistrationError::kDuplicateTriggerData),
      },
      {
          .desc = "trigger_data_value_maximal_length_within_spec",
          .json = R"json({"trigger_specs": [{"trigger_data": [
             0,  1,  2,  3,  4,  5,  6,  7,
             8,  9, 10, 11, 12, 13, 14, 15,
            16, 17, 18, 19, 20, 21, 22, 23,
            24, 25, 26, 27, 28, 29, 30, 31
          ]}]})json",
          .enabled_matches = ValueIs(SizeIs(32)),
      },
      {
          .desc = "trigger_data_value_maximal_length_across_specs",
          .json = R"json({"trigger_specs": [
            {"trigger_data": [ 0,  1,  2,  3,  4,  5,  6,  7]},
            {"trigger_data": [ 8,  9, 10, 11, 12, 13, 14, 15]},
            {"trigger_data": [16, 17, 18, 19, 20, 21, 22, 23]},
            {"trigger_data": [24, 25, 26, 27, 28, 29, 30, 31]}
          ]})json",
          .enabled_matches = ValueIs(SizeIs(32)),
      },
      {
          .desc = "trigger_data_invalid_for_modulus_non_contiguous",
          .json = R"json({"trigger_specs": [
            {"trigger_data": [0, 1, 2]},
            {"trigger_data": [4]}
          ]})json",
          .trigger_data_matching = TriggerDataMatching::kModulus,
          .enabled_matches = ErrorIs(
              SourceRegistrationError::kInvalidTriggerDataForMatchingMode),
      },
      {
          .desc = "trigger_data_invalid_for_modulus_start_not_zero",
          .json = R"json({"trigger_specs": [{"trigger_data": [1, 2, 3]}]})json",
          .trigger_data_matching = TriggerDataMatching::kModulus,
          .enabled_matches = ErrorIs(
              SourceRegistrationError::kInvalidTriggerDataForMatchingMode),
      },
      {
          .desc = "trigger_data_valid_for_modulus",
          .json = R"json({"trigger_specs": [
            {"trigger_data": [1, 3]},
            {"trigger_data": [2]},
            {"trigger_data": [0]}
          ]})json",
          .trigger_data_matching = TriggerDataMatching::kModulus,
          .enabled_matches = ValueIs(_),
      },
      {
          // This is tested more thoroughly in
          // `event_report_windows_unittest.cc`.
          .desc = "invalid_event_report_windows",
          .json = R"json({"trigger_specs": [{
            "trigger_data": [0],
            "event_report_windows": null
          }]})json",
          .enabled_matches =
              ErrorIs(SourceRegistrationError::kEventReportWindowsWrongType),
      },
      {
          .desc = "non_default_event_report_windows",
          .json = R"json({"trigger_specs": [{
            "trigger_data": [0],
            "event_report_windows": {"end_times": [3601]}
          }]})json",
          .enabled_matches = ValueIs(ElementsAre(
              Pair(_, Property(&TriggerSpec::event_report_windows,
                               Property(&EventReportWindows::end_times,
                                        ElementsAre(base::Seconds(3601))))))),
      },
      {
          .desc = "singular_event_report_window_ignored",
          .json = R"json({"trigger_specs": [{
            "trigger_data": [0],
            "event_report_window": 3601
          }]})json",
          .enabled_matches =
              ValueIs(ElementsAre(Pair(_, TriggerSpec(kDefaultReportWindows)))),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    const base::Value::Dict dict = base::test::ParseJsonDict(test_case.json);

    {
      SCOPED_TRACE("disabled");
      EXPECT_THAT(TriggerSpecs::Parse(dict, test_case.source_type, kExpiry,
                                      kDefaultReportWindows,
                                      test_case.trigger_data_matching),
                  ValueIs(TriggerSpecs::Default(test_case.source_type,
                                                kDefaultReportWindows)));
    }

    {
      SCOPED_TRACE("enabled");

      base::test::ScopedFeatureList scoped_feature_list(
          features::kAttributionReportingTriggerConfig);

      EXPECT_THAT(TriggerSpecs::Parse(dict, test_case.source_type, kExpiry,
                                      kDefaultReportWindows,
                                      test_case.trigger_data_matching),
                  test_case.enabled_matches);
    }
  }
}

TEST(TriggerSpecsTest, ToJson) {
  const std::vector<TriggerSpec> kSpecList = {
      TriggerSpec(*EventReportWindows::Create(
          /*start_time=*/base::Seconds(5),
          /*end_times=*/{base::Seconds(3601)})),
      TriggerSpec(*EventReportWindows::Create(
          /*start_time=*/base::Seconds(2),
          /*end_times=*/{base::Seconds(4601)})),
  };

  const auto kSpecs = TriggerSpecs::CreateForTesting(
      /*trigger_data_indices=*/
      {
          {/*trigger_data=*/1, /*index=*/0},
          {/*trigger_data=*/5, /*index=*/0},
          {/*trigger_data=*/3, /*index=*/1},
          {/*trigger_data=*/4294967295, /*index=*/1},
      },
      kSpecList);

  base::Value::Dict dict;
  kSpecs.Serialize(dict);

  EXPECT_THAT(dict, base::test::IsJson(R"json({
    "trigger_specs": [
      {
        "trigger_data": [1, 5],
        "event_report_windows": { "start_time": 5, "end_times": [3601] }
      },
      {
        "trigger_data": [3, 4294967295],
        "event_report_windows": { "start_time": 2, "end_times": [4601] }
      }
    ]
  })json"));
}

TEST(TriggerSpecsTest, Iterator) {
  {
    const TriggerSpecs specs;
    EXPECT_TRUE(specs.empty());
    EXPECT_EQ(specs.size(), 0u);
    EXPECT_TRUE(specs.begin() == specs.end());
  }

  const std::vector<TriggerSpec> kSpecList = {
      TriggerSpec(*EventReportWindows::Create(
          /*start_time=*/base::Seconds(0),
          /*end_times=*/{base::Seconds(3601)})),
      TriggerSpec(*EventReportWindows::Create(
          /*start_time=*/base::Seconds(0),
          /*end_times=*/{base::Seconds(4601)})),
  };

  const auto kSpecs = TriggerSpecs::CreateForTesting(
      /*trigger_data_indices=*/
      {
          {/*trigger_data=*/1, /*index=*/0},
          {/*trigger_data=*/5, /*index=*/0},
          {/*trigger_data=*/3, /*index=*/1},
          {/*trigger_data=*/4294967295, /*index=*/1},
      },
      kSpecList);

  EXPECT_FALSE(kSpecs.empty());
  EXPECT_EQ(kSpecs.size(), 4u);

  auto it = kSpecs.begin();
  ASSERT_TRUE(it != kSpecs.end());

  EXPECT_EQ(it.index(), 0u);
  EXPECT_THAT(*it, Pair(1, kSpecList[0]));

  it++;

  EXPECT_EQ(it.index(), 1u);
  EXPECT_THAT(*it, Pair(3, kSpecList[1]));

  it++;

  EXPECT_EQ(it.index(), 2u);
  EXPECT_THAT(*it, Pair(5, kSpecList[0]));

  it++;

  EXPECT_EQ(it.index(), 3u);
  EXPECT_THAT(*it, Pair(4294967295, kSpecList[1]));

  EXPECT_TRUE(++it == kSpecs.end());
}

}  // namespace
}  // namespace attribution_reporting
