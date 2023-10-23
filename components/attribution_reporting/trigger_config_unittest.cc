// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_config.h"

#include <utility>

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_data_matching.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::TriggerDataMatching;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::Property;

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

}  // namespace
}  // namespace attribution_reporting
