// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/max_event_level_reports.h"

#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SourceType;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;

TEST(MaxEventLevelReportsTest, Parse) {
  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<base::expected<int, SourceRegistrationError>> matches;
    SourceType source_type = SourceType::kNavigation;
  } kTestCases[] = {
      {
          "omitted_event",
          R"json({})json",
          ValueIs(1),
          SourceType::kEvent,
      },
      {
          "omitted_navigation",
          R"json({})json",
          ValueIs(3),
          SourceType::kNavigation,
      },
      {
          "valid",
          R"json({"max_event_level_reports": 5})json",
          ValueIs(5),
      },
      {
          "wrong_type",
          R"json({"max_event_level_reports": "5"})json",
          ErrorIs(SourceRegistrationError::kMaxEventLevelReportsValueInvalid),
      },
      {
          "negative",
          R"json({"max_event_level_reports": -5})json",
          ErrorIs(SourceRegistrationError::kMaxEventLevelReportsValueInvalid),
      },
      {
          "zero",
          R"json({"max_event_level_reports": 0})json",
          ValueIs(0),
      },
      {
          "max",
          R"json({"max_event_level_reports": 20})json",
          ValueIs(20),
      },
      {
          "higher_than_max",
          R"json({"max_event_level_reports": 21})json",
          ErrorIs(SourceRegistrationError::kMaxEventLevelReportsValueInvalid),
      },
      {
          "non_integer",
          R"json({"max_event_level_reports": 5.1})json",
          ErrorIs(SourceRegistrationError::kMaxEventLevelReportsValueInvalid),
      },
      {
          "integer_with_trailing_zero",
          R"json({"max_event_level_reports": 5.0})json",
          ValueIs(5),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    base::Value::Dict input = base::test::ParseJsonDict(test_case.json);

    EXPECT_THAT(MaxEventLevelReports::Parse(input, test_case.source_type),
                test_case.matches);
  }
}

TEST(MaxEventLevelReportsTest, Serialize) {
  base::Value::Dict dict;
  MaxEventLevelReports(5).Serialize(dict);

  EXPECT_THAT(dict,
              base::test::IsJson(R"json({"max_event_level_reports": 5})json"));
}

}  // namespace
}  // namespace attribution_reporting
