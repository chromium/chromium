// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_config.h"

#include <stdint.h>

#include <limits>
#include <utility>

#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
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
using ::testing::IsEmpty;
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

TEST(TriggerDataSetTest, Default) {
  EXPECT_THAT(TriggerDataSet(SourceType::kEvent),
              Property(&TriggerDataSet::trigger_data, ElementsAre(0, 1)));

  EXPECT_THAT(TriggerDataSet(SourceType::kNavigation),
              Property(&TriggerDataSet::trigger_data,
                       ElementsAre(0, 1, 2, 3, 4, 5, 6, 7)));
}

TEST(TriggerDataSetTest, Parse) {
  const struct {
    const char* desc;
    const char* json;
    SourceType source_type = SourceType::kNavigation;
    TriggerDataMatching trigger_data_matching = TriggerDataMatching::kExact;

    ::testing::Matcher<base::expected<TriggerDataSet, SourceRegistrationError>>
        matches_top_level_trigger_data;
  } kTestCases[] = {
      {
          .desc = "missing_navigation",
          .json = R"json({})json",
          .source_type = SourceType::kNavigation,
          .matches_top_level_trigger_data =
              ValueIs(TriggerDataSet(SourceType::kNavigation)),
      },
      {
          .desc = "missing_event",
          .json = R"json({})json",
          .source_type = SourceType::kEvent,
          .matches_top_level_trigger_data =
              ValueIs(TriggerDataSet(SourceType::kEvent)),
      },
      {
          .desc = "trigger_data_wrong_type",
          .json = R"json({"trigger_data": 1})json",
          .matches_top_level_trigger_data =
              ErrorIs(SourceRegistrationError::kTriggerDataListInvalid),
      },
      {
          .desc = "trigger_data_empty",
          .json = R"json({"trigger_data": []})json",
          .matches_top_level_trigger_data =
              ValueIs(Property(&TriggerDataSet::trigger_data, IsEmpty())),
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
          .matches_top_level_trigger_data =
              ErrorIs(SourceRegistrationError::kExcessiveTriggerData),
      },
      {
          .desc = "trigger_data_value_wrong_type",
          .json = R"json({"trigger_data": ["1"]})json",
          .matches_top_level_trigger_data =
              ErrorIs(SourceRegistrationError::kTriggerDataListInvalid),
      },
      {
          .desc = "trigger_data_value_fractional",
          .json = R"json({"trigger_data": [1.5]})json",
          .matches_top_level_trigger_data =
              ErrorIs(SourceRegistrationError::kTriggerDataListInvalid),
      },
      {
          .desc = "trigger_data_value_negative",
          .json = R"json({"trigger_data": [-1]})json",
          .matches_top_level_trigger_data =
              ErrorIs(SourceRegistrationError::kTriggerDataListInvalid),
      },
      {
          .desc = "trigger_data_value_above_max",
          .json = R"json({"trigger_data": [4294967296]})json",
          .matches_top_level_trigger_data =
              ErrorIs(SourceRegistrationError::kTriggerDataListInvalid),
      },
      {
          .desc = "trigger_data_value_minimal",
          .json = R"json({"trigger_data": [0]})json",
          .matches_top_level_trigger_data =
              ValueIs(Property(&TriggerDataSet::trigger_data, ElementsAre(0))),
      },
      {
          .desc = "trigger_data_value_maximal",
          .json = R"json({"trigger_data": [4294967295]})json",
          .matches_top_level_trigger_data = ValueIs(
              Property(&TriggerDataSet::trigger_data, ElementsAre(4294967295))),
      },
      {
          .desc = "trigger_data_value_trailing_zero",
          .json = R"json({"trigger_data": [2.0]})json",
          .matches_top_level_trigger_data =
              ValueIs(Property(&TriggerDataSet::trigger_data, ElementsAre(2))),
      },
      {
          .desc = "trigger_data_value_duplicate",
          .json = R"json({"trigger_data": [1, 3, 1, 2]})json",
          .matches_top_level_trigger_data =
              ErrorIs(SourceRegistrationError::kDuplicateTriggerData),
      },
      {
          .desc = "trigger_data_maximal_length",
          .json = R"json({"trigger_data": [
             0,  1,  2,  3,  4,  5,  6,  7,
             8,  9, 10, 11, 12, 13, 14, 15,
            16, 17, 18, 19, 20, 21, 22, 23,
            24, 25, 26, 27, 28, 29, 30, 31
          ]})json",
          .matches_top_level_trigger_data =
              ValueIs(Property(&TriggerDataSet::trigger_data, SizeIs(32))),
      },
      {
          .desc = "trigger_data_invalid_for_modulus_non_contiguous",
          .json = R"json({"trigger_data": [0, 2]})json",
          .trigger_data_matching = TriggerDataMatching::kModulus,
          .matches_top_level_trigger_data = ErrorIs(
              SourceRegistrationError::kInvalidTriggerDataForMatchingMode),
      },
      {
          .desc = "trigger_data_invalid_for_modulus_start_not_zero",
          .json = R"json({"trigger_data": [1, 2, 3]})json",
          .trigger_data_matching = TriggerDataMatching::kModulus,
          .matches_top_level_trigger_data = ErrorIs(
              SourceRegistrationError::kInvalidTriggerDataForMatchingMode),
      },
      {
          .desc = "trigger_data_valid_for_modulus",
          .json = R"json({"trigger_data": [1, 3, 2, 0]})json",
          .trigger_data_matching = TriggerDataMatching::kModulus,
          .matches_top_level_trigger_data = ValueIs(_),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    const base::Value::Dict dict = base::test::ParseJsonDict(test_case.json);

    EXPECT_THAT(TriggerDataSet::Parse(dict, test_case.source_type,
                                      test_case.trigger_data_matching),
                test_case.matches_top_level_trigger_data);
  }
}

TEST(TriggerDataSetTest, ToJson) {
  const auto kSet = *TriggerDataSet::Create(
      /*trigger_data=*/{1, 5, 3, 4294967295});

  base::Value::Dict dict;
  kSet.Serialize(dict);

  EXPECT_THAT(dict, base::test::IsJson(R"json({
    "trigger_data": [1, 3, 5, 4294967295]
  })json"));
}

TEST(TriggerDataSetTest, Find) {
  {
    const TriggerDataSet kSet;

    EXPECT_FALSE(kSet.find(/*trigger_data=*/1, TriggerDataMatching::kExact));
    EXPECT_FALSE(kSet.find(/*trigger_data=*/1, TriggerDataMatching::kModulus));
  }

  const auto kSet = *TriggerDataSet::Create(
      /*trigger_data=*/{1, 3, 4, 5});

  const struct {
    TriggerDataMatching trigger_data_matching;
    uint64_t trigger_data;
    std::optional<uint32_t> expected;
  } kTestCases[] = {
      {TriggerDataMatching::kExact, 0, std::nullopt},
      {TriggerDataMatching::kExact, 1, 1},
      {TriggerDataMatching::kExact, 2, std::nullopt},
      {TriggerDataMatching::kExact, 3, 3},
      {TriggerDataMatching::kExact, 4, 4},
      {TriggerDataMatching::kExact, 5, 5},
      {TriggerDataMatching::kExact, 6, std::nullopt},
      {TriggerDataMatching::kExact, std::numeric_limits<uint64_t>::max(),
       std::nullopt},

      {TriggerDataMatching::kModulus, 0, 1},
      {TriggerDataMatching::kModulus, 1, 3},
      {TriggerDataMatching::kModulus, 2, 4},
      {TriggerDataMatching::kModulus, 3, 5},
      {TriggerDataMatching::kModulus, 4, 1},
      {TriggerDataMatching::kModulus, 5, 3},
      {TriggerDataMatching::kModulus, 6, 4},
      // uint64 max % 4 == 3; trigger data 5 is at index 3
      {TriggerDataMatching::kModulus, std::numeric_limits<uint64_t>::max(), 5},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.trigger_data_matching);
    SCOPED_TRACE(test_case.trigger_data);

    EXPECT_EQ(
        kSet.find(test_case.trigger_data, test_case.trigger_data_matching),
        test_case.expected);
  }
}

// Technically redundant with `TriggerDataSetTest.Find`, but included to
// demonstrate the expected behavior for real-world trigger data, of which
// `TriggerDataSet()` can return a subset.
TEST(TriggerDataSetTest, Find_ModulusContiguous) {
  const auto kSet = *TriggerDataSet::Create(
      /*trigger_data=*/{0, 1, 2});

  const struct {
    uint64_t trigger_data;
    std::optional<uint32_t> expected;
    bool expected_metric_value;
  } kTestCases[] = {
      {0, 0, true},  {1, 1, true},  {2, 2, true},
      {3, 0, false}, {4, 1, false}, {5, 2, false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.trigger_data);

    EXPECT_EQ(kSet.find(test_case.trigger_data, TriggerDataMatching::kModulus),
              test_case.expected);
  }
}

}  // namespace
}  // namespace attribution_reporting
