// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_config.h"

#include <stdint.h>

#include <limits>
#include <utility>

#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/max_event_level_reports.h"
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
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::IsTrue;
using ::testing::Key;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Property;
using ::testing::SizeIs;

TEST(TriggerDataMatchingTest, Parse) {
  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<
        base::expected<TriggerDataMatching, SourceRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          .desc = "missing",
          .json = R"json({})json",
          .matches = ValueIs(TriggerDataMatching::kModulus),
      },
      {
          .desc = "wrong_type",
          .json = R"json({"trigger_data_matching":1})json",
          .matches = ErrorIs(
              SourceRegistrationError::kTriggerDataMatchingValueInvalid),
      },
      {
          .desc = "invalid_value",
          .json = R"json({"trigger_data_matching":"MODULUS"})json",
          .matches = ErrorIs(
              SourceRegistrationError::kTriggerDataMatchingValueInvalid),
      },
      {
          .desc = "valid_modulus",
          .json = R"json({"trigger_data_matching":"modulus"})json",
          .matches = ValueIs(TriggerDataMatching::kModulus),
      },
      {
          .desc = "valid_exact",
          .json = R"json({"trigger_data_matching":"exact"})json",
          .matches = ValueIs(TriggerDataMatching::kExact),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    const base::Value::Dict dict = base::test::ParseJsonDict(test_case.json);

    EXPECT_THAT(ParseTriggerDataMatching(dict), test_case.matches);
  }
}

TEST(TriggerDataMatchingTest, Serialize) {
  const struct {
    TriggerDataMatching trigger_data_matching;
    base::Value::Dict expected;
  } kTestCases[] = {
      {
          TriggerDataMatching::kModulus,
          base::Value::Dict().Set("trigger_data_matching", "modulus"),
      },
      {
          TriggerDataMatching::kExact,
          base::Value::Dict().Set("trigger_data_matching", "exact"),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.trigger_data_matching);

    base::Value::Dict dict;
    Serialize(dict, test_case.trigger_data_matching);
    EXPECT_EQ(dict, test_case.expected);
  }
}

TEST(TriggerSpecsTest, Default) {
  const auto kReportWindows = *EventReportWindows::Create(
      /*start_time=*/base::Hours(2),
      /*end_times=*/{base::Hours(9)});

  EXPECT_THAT(TriggerSpecs(SourceType::kEvent, kReportWindows,
                           MaxEventLevelReports(SourceType::kEvent)),
              AllOf(Property(&TriggerSpecs::SingleSharedSpec, IsTrue()),
                    ElementsAre(Pair(0, TriggerSpec(kReportWindows)),
                                Pair(1, TriggerSpec(kReportWindows)))));

  EXPECT_THAT(TriggerSpecs(SourceType::kNavigation, kReportWindows,
                           MaxEventLevelReports(SourceType::kNavigation)),
              AllOf(Property(&TriggerSpecs::SingleSharedSpec, IsTrue()),
                    ElementsAre(Pair(0, TriggerSpec(kReportWindows)),
                                Pair(1, TriggerSpec(kReportWindows)),
                                Pair(2, TriggerSpec(kReportWindows)),
                                Pair(3, TriggerSpec(kReportWindows)),
                                Pair(4, TriggerSpec(kReportWindows)),
                                Pair(5, TriggerSpec(kReportWindows)),
                                Pair(6, TriggerSpec(kReportWindows)),
                                Pair(7, TriggerSpec(kReportWindows)))));
}

TEST(TriggerSpecsTest, Parse) {
  constexpr base::TimeDelta kExpiry = base::Days(30);
  const EventReportWindows kDefaultReportWindows;

  const struct {
    const char* desc;
    const char* json;
    SourceType source_type = SourceType::kNavigation;
    TriggerDataMatching trigger_data_matching = TriggerDataMatching::kExact;

    ::testing::Matcher<base::expected<TriggerSpecs, SourceRegistrationError>>
        matches_full_flex;

    ::testing::Matcher<base::expected<TriggerSpecs, SourceRegistrationError>>
        matches_top_level_trigger_data;
  } kTestCases[] = {
      {
          .desc = "missing_navigation",
          .json = R"json({})json",
          .source_type = SourceType::kNavigation,
          .matches_full_flex = ValueIs(
              TriggerSpecs(SourceType::kNavigation, kDefaultReportWindows,
                           MaxEventLevelReports(SourceType::kNavigation))),
          .matches_top_level_trigger_data = ValueIs(
              TriggerSpecs(SourceType::kNavigation, kDefaultReportWindows,
                           MaxEventLevelReports(SourceType::kNavigation))),
      },
      {
          .desc = "missing_event",
          .json = R"json({})json",
          .source_type = SourceType::kEvent,
          .matches_full_flex =
              ValueIs(TriggerSpecs(SourceType::kEvent, kDefaultReportWindows,
                                   MaxEventLevelReports(SourceType::kEvent))),
          .matches_top_level_trigger_data =
              ValueIs(TriggerSpecs(SourceType::kEvent, kDefaultReportWindows,
                                   MaxEventLevelReports(SourceType::kEvent))),
      },
      {
          .desc = "trigger_specs_wrong_type",
          .json = R"json({"trigger_specs": 1})json",
          .matches_full_flex =
              ErrorIs(SourceRegistrationError::kTriggerSpecsWrongType),
          .matches_top_level_trigger_data = ValueIs(_),
      },
      {
          .desc = "trigger_specs_empty",
          .json = R"json({"trigger_specs": []})json",
          .matches_full_flex = ValueIs(
              AllOf(Property(&TriggerSpecs::empty, true),
                    Property(&TriggerSpecs::size, 0u),
                    Property(&TriggerSpecs::SingleSharedSpec, IsNull()),  //
                    IsEmpty())),
          .matches_top_level_trigger_data = ValueIs(_),
      },
      {
          .desc = "trigger_specs_too_long",
          .json = R"json({"trigger_specs": [
             0,  1,  2,  3,  4,  5,  6,  7,
             8,  9, 10, 11, 12, 13, 14, 15,
            16, 17, 18, 19, 20, 21, 22, 23,
            24, 25, 26, 27, 28, 29, 30, 31,
            32
          ]})json",
          .matches_full_flex = ErrorIs(
              SourceRegistrationError::kTriggerSpecExcessiveTriggerData),
          .matches_top_level_trigger_data = ValueIs(_),
      },
      {
          .desc = "spec_wrong_type",
          .json = R"json({"trigger_specs": [0]})json",
          .matches_full_flex =
              ErrorIs(SourceRegistrationError::kTriggerSpecsWrongType),
          .matches_top_level_trigger_data = ValueIs(_),
      },
      {
          .desc = "spec_trigger_data_missing",
          .json = R"json({"trigger_specs": [{}]})json",
          .matches_full_flex =
              ErrorIs(SourceRegistrationError::kTriggerSpecTriggerDataMissing),
          .matches_top_level_trigger_data = ValueIs(_),
      },
      {
          .desc = "trigger_data_wrong_type",
          .json = R"json({"trigger_data": 1})json",
          .matches_full_flex =
              ErrorIs(SourceRegistrationError::kTriggerDataListInvalid),
          .matches_top_level_trigger_data =
              ErrorIs(SourceRegistrationError::kTriggerDataListInvalid),
      },
      {
          .desc = "spec_trigger_data_empty",
          .json = R"json({"trigger_specs": [{"trigger_data": []}]})json",
          .matches_full_flex = ErrorIs(
              SourceRegistrationError::kTriggerSpecTriggerDataListInvalid),
          .matches_top_level_trigger_data = ValueIs(_),
      },
      {
          .desc = "trigger_data_empty",
          .json = R"json({"trigger_data": []})json",
          .matches_full_flex = ValueIs(IsEmpty()),
          .matches_top_level_trigger_data = ValueIs(
              AllOf(Property(&TriggerSpecs::empty, true),
                    Property(&TriggerSpecs::size, 0u),
                    Property(&TriggerSpecs::SingleSharedSpec, IsNull()),  //
                    IsEmpty())),
      },
      {
          .desc = "trigger_data_too_long",
          .json = R"json({"trigger_data": [
             0,  1,  2,  3,  4,  5,  6,  7,
             8,  9, 10, 11, 12, 13, 14, 15,
            16, 17, 18, 19, 20, 21, 22, 23,
            24, 25, 26, 27, 28, 29, 30, 31,
            32
          ]})json",
          .matches_full_flex =
              ErrorIs(SourceRegistrationError::kExcessiveTriggerData),
          .matches_top_level_trigger_data =
              ErrorIs(SourceRegistrationError::kExcessiveTriggerData),
      },
      {
          .desc = "trigger_data_value_too_long_across_specs",
          .json = R"json({"trigger_specs": [
            {"trigger_data": [
               0,  1,  2,  3,  4,  5,  6,  7,
               8,  9, 10, 11, 12, 13, 14, 15,
              16, 17, 18, 19, 20, 21, 22, 23,
              24, 25, 26, 27, 28, 29, 30, 31
            ]},
            {"trigger_data": [32]}
          ]})json",
          .matches_full_flex = ErrorIs(
              SourceRegistrationError::kTriggerSpecExcessiveTriggerData),
          .matches_top_level_trigger_data = ValueIs(_),
      },
      {
          .desc = "trigger_data_value_wrong_type",
          .json = R"json({"trigger_data": ["1"]})json",
          .matches_full_flex =
              ErrorIs(SourceRegistrationError::kTriggerDataListInvalid),
          .matches_top_level_trigger_data =
              ErrorIs(SourceRegistrationError::kTriggerDataListInvalid),
      },
      {
          .desc = "trigger_data_value_fractional",
          .json = R"json({"trigger_data": [1.5]})json",
          .matches_full_flex =
              ErrorIs(SourceRegistrationError::kTriggerDataListInvalid),
          .matches_top_level_trigger_data =
              ErrorIs(SourceRegistrationError::kTriggerDataListInvalid),
      },
      {
          .desc = "trigger_data_value_negative",
          .json = R"json({"trigger_data": [-1]})json",
          .matches_full_flex =
              ErrorIs(SourceRegistrationError::kTriggerDataListInvalid),
          .matches_top_level_trigger_data =
              ErrorIs(SourceRegistrationError::kTriggerDataListInvalid),
      },
      {
          .desc = "trigger_data_value_above_max",
          .json = R"json({"trigger_data": [4294967296]})json",
          .matches_full_flex =
              ErrorIs(SourceRegistrationError::kTriggerDataListInvalid),
          .matches_top_level_trigger_data =
              ErrorIs(SourceRegistrationError::kTriggerDataListInvalid),
      },
      {
          .desc = "trigger_data_value_minimal",
          .json = R"json({"trigger_data": [0]})json",
          .matches_full_flex = ValueIs(ElementsAre(Key(0))),
          .matches_top_level_trigger_data =
              ValueIs(AllOf(Property(&TriggerSpecs::empty, false),
                            Property(&TriggerSpecs::size, 1u),
                            Property(&TriggerSpecs::SingleSharedSpec, IsTrue()),
                            ElementsAre(Key(0)))),
      },
      {
          .desc = "trigger_data_value_maximal",
          .json = R"json({"trigger_data": [4294967295]})json",
          .matches_full_flex = ValueIs(ElementsAre(Key(4294967295))),
          .matches_top_level_trigger_data =
              ValueIs(ElementsAre(Key(4294967295))),
      },
      {
          .desc = "trigger_data_value_trailing_zero",
          .json = R"json({"trigger_data": [2.0]})json",
          .matches_full_flex = ValueIs(ElementsAre(Key(2))),
          .matches_top_level_trigger_data = ValueIs(ElementsAre(Key(2))),
      },
      {
          .desc = "trigger_data_value_duplicate",
          .json = R"json({"trigger_data": [1, 3, 1, 2]})json",
          .matches_full_flex =
              ErrorIs(SourceRegistrationError::kDuplicateTriggerData),
          .matches_top_level_trigger_data =
              ErrorIs(SourceRegistrationError::kDuplicateTriggerData),
      },
      {
          .desc = "trigger_data_value_duplicate_across_specs",
          .json = R"json({"trigger_specs": [
            {"trigger_data": [1, 3]},
            {"trigger_data": [4, 2]},
            {"trigger_data": [1, 5]},
          ]})json",
          .matches_full_flex = ErrorIs(
              SourceRegistrationError::kTriggerSpecDuplicateTriggerData),
          .matches_top_level_trigger_data = ValueIs(_),
      },
      {
          .desc = "trigger_data_maximal_length",
          .json = R"json({"trigger_data": [
             0,  1,  2,  3,  4,  5,  6,  7,
             8,  9, 10, 11, 12, 13, 14, 15,
            16, 17, 18, 19, 20, 21, 22, 23,
            24, 25, 26, 27, 28, 29, 30, 31
          ]})json",
          .matches_full_flex = ValueIs(SizeIs(32)),
          .matches_top_level_trigger_data = ValueIs(SizeIs(32)),
      },
      {
          .desc = "trigger_data_value_maximal_length_across_specs",
          .json = R"json({"trigger_specs": [
            {"trigger_data": [ 0,  1,  2,  3,  4,  5,  6,  7]},
            {"trigger_data": [ 8,  9, 10, 11, 12, 13, 14, 15]},
            {"trigger_data": [16, 17, 18, 19, 20, 21, 22, 23]},
            {"trigger_data": [24, 25, 26, 27, 28, 29, 30, 31]}
          ]})json",
          .matches_full_flex = ValueIs(SizeIs(32)),
          .matches_top_level_trigger_data = ValueIs(_),
      },
      {
          .desc = "trigger_data_invalid_for_modulus_non_contiguous",
          .json = R"json({"trigger_data": [0, 2]})json",
          .trigger_data_matching = TriggerDataMatching::kModulus,
          .matches_full_flex = ErrorIs(
              SourceRegistrationError::kInvalidTriggerDataForMatchingMode),
          .matches_top_level_trigger_data = ErrorIs(
              SourceRegistrationError::kInvalidTriggerDataForMatchingMode),
      },
      {
          .desc = "trigger_data_invalid_for_modulus_start_not_zero",
          .json = R"json({"trigger_data": [1, 2, 3]})json",
          .trigger_data_matching = TriggerDataMatching::kModulus,
          .matches_full_flex = ErrorIs(
              SourceRegistrationError::kInvalidTriggerDataForMatchingMode),
          .matches_top_level_trigger_data = ErrorIs(
              SourceRegistrationError::kInvalidTriggerDataForMatchingMode),
      },
      {
          .desc = "trigger_data_valid_for_modulus",
          .json = R"json({"trigger_data": [1, 3, 2, 0]})json",
          .trigger_data_matching = TriggerDataMatching::kModulus,
          .matches_full_flex = ValueIs(_),
          .matches_top_level_trigger_data = ValueIs(_),
      },
      {
          .desc = "trigger_data_valid_for_modulus_across_specs",
          .json = R"json({"trigger_specs": [
            {"trigger_data": [1, 3]},
            {"trigger_data": [2]},
            {"trigger_data": [0]}
          ]})json",
          .trigger_data_matching = TriggerDataMatching::kModulus,
          .matches_full_flex = ValueIs(_),
          .matches_top_level_trigger_data = ValueIs(_),
      },
      {
          // This is tested more thoroughly in
          // `event_report_windows_unittest.cc`.
          .desc = "invalid_event_report_windows",
          .json = R"json({"trigger_specs": [{
            "trigger_data": [0],
            "event_report_windows": null
          }]})json",
          .matches_full_flex =
              ErrorIs(SourceRegistrationError::kEventReportWindowsWrongType),
          .matches_top_level_trigger_data = ValueIs(_),
      },
      {
          .desc = "non_default_event_report_windows",
          .json = R"json({"trigger_specs": [{
            "trigger_data": [0],
            "event_report_windows": {"end_times": [3601]}
          }]})json",
          .matches_full_flex = ValueIs(ElementsAre(
              Pair(_, Property(&TriggerSpec::event_report_windows,
                               Property(&EventReportWindows::end_times,
                                        ElementsAre(base::Seconds(3601))))))),
          .matches_top_level_trigger_data = ValueIs(_),
      },
      {
          .desc = "singular_event_report_window_ignored",
          .json = R"json({"trigger_specs": [{
            "trigger_data": [0],
            "event_report_window": 3601
          }]})json",
          .matches_full_flex =
              ValueIs(ElementsAre(Pair(_, TriggerSpec(kDefaultReportWindows)))),
          .matches_top_level_trigger_data = ValueIs(_),
      },
      {
          .desc = "top_level_trigger_data_and_trigger_specs",
          .json = R"json({"trigger_data": [], "trigger_specs": []})json",
          .matches_full_flex = ErrorIs(
              SourceRegistrationError::kTopLevelTriggerDataAndTriggerSpecs),
          .matches_top_level_trigger_data = ValueIs(IsEmpty()),
      },
      {
          // Tested more thoroughly in `max_event_level_reports_unittest.cc`
          .desc = "max_event_level_reports_valid",
          .json = R"json({"max_event_level_reports":5})json",
          .matches_full_flex =
              ValueIs(Property(&TriggerSpecs::max_event_level_reports, 5)),
          .matches_top_level_trigger_data =
              ValueIs(Property(&TriggerSpecs::max_event_level_reports, 5)),
      },
      {
          // Tested more thoroughly in `max_event_level_reports_unittest.cc`
          .desc = "max_event_level_reports_invalid",
          .json = R"json({"max_event_level_reports":null})json",
          .matches_full_flex = ErrorIs(
              SourceRegistrationError::kMaxEventLevelReportsValueInvalid),
          .matches_top_level_trigger_data = ErrorIs(
              SourceRegistrationError::kMaxEventLevelReportsValueInvalid),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    const base::Value::Dict dict = base::test::ParseJsonDict(test_case.json);

    {
      SCOPED_TRACE("full_flex");
      EXPECT_THAT(TriggerSpecs::ParseFullFlexForTesting(
                      dict, test_case.source_type, kExpiry,
                      kDefaultReportWindows, test_case.trigger_data_matching),
                  test_case.matches_full_flex);
    }

    {
      SCOPED_TRACE("top_level_trigger_data");

      EXPECT_THAT(TriggerSpecs::ParseTopLevelTriggerData(
                      dict, test_case.source_type, kDefaultReportWindows,
                      test_case.trigger_data_matching),
                  test_case.matches_top_level_trigger_data);
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

  const auto kSpecs = *TriggerSpecs::Create(
      /*trigger_data_indices=*/
      {
          {/*trigger_data=*/1, /*index=*/0},
          {/*trigger_data=*/5, /*index=*/0},
          {/*trigger_data=*/3, /*index=*/1},
          {/*trigger_data=*/4294967295, /*index=*/1},
      },
      kSpecList, MaxEventLevelReports(7));

  base::Value::Dict dict;
  kSpecs.Serialize(dict);

  EXPECT_THAT(dict, base::test::IsJson(R"json({
    "max_event_level_reports": 7,
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

  const auto kSpecs = *TriggerSpecs::Create(
      /*trigger_data_indices=*/
      {
          {/*trigger_data=*/1, /*index=*/0},
          {/*trigger_data=*/5, /*index=*/0},
          {/*trigger_data=*/3, /*index=*/1},
          {/*trigger_data=*/4294967295, /*index=*/1},
      },
      kSpecList, MaxEventLevelReports());

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

TEST(TriggerSpecsTest, Find) {
  {
    const TriggerSpecs kSpecs;

    EXPECT_FALSE(kSpecs.find(/*trigger_data=*/1, TriggerDataMatching::kExact));
    EXPECT_FALSE(
        kSpecs.find(/*trigger_data=*/1, TriggerDataMatching::kModulus));
  }

  const std::vector<TriggerSpec> kSpecList = {
      TriggerSpec(*EventReportWindows::Create(
          /*start_time=*/base::Seconds(0),
          /*end_times=*/{base::Seconds(3601)})),
      TriggerSpec(*EventReportWindows::Create(
          /*start_time=*/base::Seconds(1),
          /*end_times=*/{base::Seconds(4601)})),
  };

  const auto kSpecs = *TriggerSpecs::Create(
      /*trigger_data_indices=*/
      {
          {/*trigger_data=*/1, /*index=*/0},
          {/*trigger_data=*/3, /*index=*/1},
          {/*trigger_data=*/4, /*index=*/1},
          {/*trigger_data=*/5, /*index=*/0},
      },
      kSpecList, MaxEventLevelReports());

  const struct {
    TriggerDataMatching trigger_data_matching;
    uint64_t trigger_data;
    ::testing::Matcher<TriggerSpecs::const_iterator> matches;
  } kTestCases[] = {
      {TriggerDataMatching::kExact, 0, kSpecs.end()},
      {TriggerDataMatching::kExact, 1, Optional(Pair(1, kSpecList[0]))},
      {TriggerDataMatching::kExact, 2, kSpecs.end()},
      {TriggerDataMatching::kExact, 3, Optional(Pair(3, kSpecList[1]))},
      {TriggerDataMatching::kExact, 4, Optional(Pair(4, kSpecList[1]))},
      {TriggerDataMatching::kExact, 5, Optional(Pair(5, kSpecList[0]))},
      {TriggerDataMatching::kExact, 6, kSpecs.end()},
      {TriggerDataMatching::kExact, std::numeric_limits<uint64_t>::max(),
       kSpecs.end()},

      {TriggerDataMatching::kModulus, 0, Optional(Pair(1, kSpecList[0]))},
      {TriggerDataMatching::kModulus, 1, Optional(Pair(3, kSpecList[1]))},
      {TriggerDataMatching::kModulus, 2, Optional(Pair(4, kSpecList[1]))},
      {TriggerDataMatching::kModulus, 3, Optional(Pair(5, kSpecList[0]))},
      {TriggerDataMatching::kModulus, 4, Optional(Pair(1, kSpecList[0]))},
      {TriggerDataMatching::kModulus, 5, Optional(Pair(3, kSpecList[1]))},
      {TriggerDataMatching::kModulus, 6, Optional(Pair(4, kSpecList[1]))},
      // uint64 max % 4 == 3; trigger data 5 is at index 3
      {TriggerDataMatching::kModulus, std::numeric_limits<uint64_t>::max(),
       Optional(Pair(5, kSpecList[0]))},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.trigger_data_matching);
    SCOPED_TRACE(test_case.trigger_data);

    EXPECT_THAT(
        kSpecs.find(test_case.trigger_data, test_case.trigger_data_matching),
        test_case.matches);
  }
}

// Technically redundant with `TriggerSpecsTest.Find`, but included to
// demonstrate the expected behavior for real-world trigger specs, of which
// `TriggerSpecs()` can return a subset.
TEST(TriggerSpecsTest, Find_ModulusContiguous) {
  const std::vector<TriggerSpec> kSpecList = {
      TriggerSpec(*EventReportWindows::Create(
          /*start_time=*/base::Seconds(0),
          /*end_times=*/{base::Seconds(3601)})),
      TriggerSpec(*EventReportWindows::Create(
          /*start_time=*/base::Seconds(1),
          /*end_times=*/{base::Seconds(4601)})),
  };

  const auto kSpecs = *TriggerSpecs::Create(
      /*trigger_data_indices=*/
      {
          {/*trigger_data=*/0, /*index=*/1},
          {/*trigger_data=*/1, /*index=*/0},
          {/*trigger_data=*/2, /*index=*/1},
      },
      kSpecList, MaxEventLevelReports());

  const struct {
    uint64_t trigger_data;
    ::testing::Matcher<TriggerSpecs::const_iterator> matches;
  } kTestCases[] = {
      {0, Optional(Pair(0, kSpecList[1]))},
      {1, Optional(Pair(1, kSpecList[0]))},
      {2, Optional(Pair(2, kSpecList[1]))},
      {3, Optional(Pair(0, kSpecList[1]))},
      {4, Optional(Pair(1, kSpecList[0]))},
      {5, Optional(Pair(2, kSpecList[1]))},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.trigger_data);

    EXPECT_THAT(
        kSpecs.find(test_case.trigger_data, TriggerDataMatching::kModulus),
        test_case.matches);
  }
}

}  // namespace
}  // namespace attribution_reporting
