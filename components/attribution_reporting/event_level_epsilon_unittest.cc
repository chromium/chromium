// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/event_level_epsilon.h"

#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;

TEST(EventLevelEpsilonTest, Parse) {
  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<base::expected<double, SourceRegistrationError>> matches;
  } kTestCases[] = {
      {
          .desc = "missing",
          .json = R"json({})json",
          .matches = ValueIs(14),
      },
      {
          .desc = "wrong_type",
          .json = R"json({"event_level_epsilon": "1"})json",
          .matches =
              ErrorIs(SourceRegistrationError::kEventLevelEpsilonValueInvalid),
      },
      {
          .desc = "negative",
          .json = R"json({"event_level_epsilon": -1})json",
          .matches =
              ErrorIs(SourceRegistrationError::kEventLevelEpsilonValueInvalid),
      },
      {
          .desc = "zero",
          .json = R"json({"event_level_epsilon": 0})json",
          .matches = ValueIs(0),
      },
      {
          .desc = "max",
          .json = R"json({"event_level_epsilon": 14})json",
          .matches = ValueIs(14),
      },
      {
          .desc = "greater_than_max",
          .json = R"json({"event_level_epsilon": 14.01})json",
          .matches =
              ErrorIs(SourceRegistrationError::kEventLevelEpsilonValueInvalid),
      },
      {
          .desc = "valid",
          .json = R"json({"event_level_epsilon": 8.2})json",
          .matches = ValueIs(8.2),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    const base::Value::Dict dict = base::test::ParseJsonDict(test_case.json);

    EXPECT_THAT(EventLevelEpsilon::Parse(dict), test_case.matches);
  }
}

TEST(EventLevelEpsilonTest, Serialize) {
  const EventLevelEpsilon kEpsilon(8.2);

  base::Value::Dict dict;
  kEpsilon.Serialize(dict);
  EXPECT_EQ(dict, base::Value::Dict().Set("event_level_epsilon", 8.2));
}

}  // namespace
}  // namespace attribution_reporting
