// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/os_registration.h"

#include <string_view>
#include <tuple>
#include <vector>

#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/os_registration_error.mojom-shared.h"
#include "components/attribution_reporting/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::OsRegistrationError;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::ElementsAre;

TEST(OsRegistration, ParseOsSourceOrTriggerHeader) {
  const struct {
    const char* description;
    std::string_view header;
    ::testing::Matcher<
        base::expected<std::vector<OsRegistrationItem>, OsRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "empty",
          "",
          ErrorIs(OsRegistrationError::kInvalidList),
      },
      {
          "invalid_url",
          R"("foo")",
          ErrorIs(OsRegistrationError::kInvalidList),
      },
      {
          "integer",
          "123",
          ErrorIs(OsRegistrationError::kInvalidList),
      },
      {
          "token",
          "d",
          ErrorIs(OsRegistrationError::kInvalidList),
      },
      {
          "byte_sequence",
          ":YWJj:",
          ErrorIs(OsRegistrationError::kInvalidList),
      },
      {
          "valid_url_no_params",
          R"("https://d.test")",
          ValueIs(
              ElementsAre(OsRegistrationItem{.url = GURL("https://d.test")})),
      },
      {
          "extra_params_ignored",
          R"("https://d.test"; y=1)",
          ValueIs(
              ElementsAre(OsRegistrationItem{.url = GURL("https://d.test")})),
      },
      {
          "inner_list",
          R"(("https://d.test"))",
          ErrorIs(OsRegistrationError::kInvalidList),
      },
      {
          "multiple",
          R"(123, "https://d.test", "", "https://e.test")",
          ValueIs(
              ElementsAre(OsRegistrationItem{.url = GURL("https://d.test")},
                          OsRegistrationItem{.url = GURL("https://e.test")})),
      },
      {
          "debug_reporting_param",
          R"("https://d.test", "https://e.test";debug-reporting, "https://f.test";debug-reporting=?0)",
          ValueIs(
              ElementsAre(OsRegistrationItem{.url = GURL("https://d.test")},
                          OsRegistrationItem{.url = GURL("https://e.test"),
                                             .debug_reporting = true},
                          OsRegistrationItem{.url = GURL("https://f.test")})),
      },
      {
          "debug_reporting_param_wrong_type",
          R"("https://d.test"; debug-reporting=1)",
          ValueIs(
              ElementsAre(OsRegistrationItem{.url = GURL("https://d.test")})),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    EXPECT_THAT(ParseOsSourceOrTriggerHeader(test_case.header),
                test_case.matches);
  }
}

TEST(OsRegistration, EmitItemsPerHeaderHistogram) {
  base::HistogramTester histogram;

  std::ignore = ParseOsSourceOrTriggerHeader(
      R"(123, "https://d.test", "", "https://e.test")");

  histogram.ExpectUniqueSample("Conversions.OsRegistrationItemsPerHeader", 2,
                               1);
}

}  // namespace
}  // namespace attribution_reporting
