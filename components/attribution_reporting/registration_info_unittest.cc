// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/registration_info.h"

#include <string_view>

#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/attribution_reporting/registrar.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace attribution_reporting {
namespace {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;

TEST(RegistrationInfoTest, ParseInfo) {
  const struct {
    const char* description;
    std::string_view header;
    ::testing::Matcher<base::expected<RegistrationInfo, RegistrationInfoError>>
        matches;
  } kTestCases[] = {
      {
          "empty",
          "",
          ValueIs(RegistrationInfo()),
      },
      {
          "all-fields",
          "preferred-platform=os,report-header-errors",
          ValueIs(RegistrationInfo{
              .preferred_platform = Registrar::kOs,
              .report_header_errors = true,
          }),
      },
      {
          "list",
          R"("foo", "bar")",
          ErrorIs(RegistrationInfoError::kRootInvalid),
      },
      {
          "prefer-web",
          "preferred-platform=web",
          ValueIs(RegistrationInfo{
              .preferred_platform = Registrar::kWeb,
          }),
      },
      {
          "prefer-os",
          "preferred-platform=os",
          ValueIs(RegistrationInfo{
              .preferred_platform = Registrar::kOs,
          }),
      },
      {
          "preferred-platform-parameter-ignored",
          "preferred-platform=os;abc",
          ValueIs(RegistrationInfo{
              .preferred_platform = Registrar::kOs,
          }),
      },
      {
          "preferred-platform-missing-value",
          "preferred-platform",
          ErrorIs(RegistrationInfoError::kInvalidPreferredPlatform),
      },
      {
          "preferred-platform-unknown-value",
          "preferred-platform=abc",
          ErrorIs(RegistrationInfoError::kInvalidPreferredPlatform),
      },
      {
          "preferred-platform-invalid-type",
          "preferred-platform=\"os\"",
          ErrorIs(RegistrationInfoError::kInvalidPreferredPlatform),
      },
      {
          "preferred-platform-inner-list",
          "preferred-platform=(foo bar)",
          ErrorIs(RegistrationInfoError::kInvalidPreferredPlatform),
      },
      {
          "unknown-field",
          "unknown",
          ValueIs(RegistrationInfo()),
      },
      {
          "report-header-errors",
          "report-header-errors",
          ValueIs(RegistrationInfo{
              .report_header_errors = true,
          }),
      },
      {
          "report-header-errors-parameter-ignored",
          "report-header-errors=?0;abc",
          ValueIs(RegistrationInfo{
              .report_header_errors = false,
          }),
      },
      {
          "report-header-errors-invalid-type",
          "report-header-errors=abc",
          ErrorIs(RegistrationInfoError::kInvalidReportHeaderErrors),
      },
      {
          "report-header-errors-inner-list",
          "report-header-errors=(foo bar)",
          ErrorIs(RegistrationInfoError::kInvalidReportHeaderErrors),
      },
  };

  static constexpr char kRegistrationInfoErrorMetric[] =
      "Conversions.RegistrationInfoError";

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);

    base::HistogramTester histograms;
    auto info = RegistrationInfo::ParseInfo(test_case.header);
    EXPECT_THAT(info, test_case.matches);
    if (info.has_value()) {
      histograms.ExpectTotalCount(kRegistrationInfoErrorMetric, 0);
    } else {
      histograms.ExpectUniqueSample(kRegistrationInfoErrorMetric, info.error(),
                                    1);
    }
  }
}

void Parses(std::string_view input) {
  std::ignore = RegistrationInfo::ParseInfo(input);
}

FUZZ_TEST(RegistrationInfoTest, Parses)
    .WithDomains(fuzztest::Arbitrary<std::string>());

}  // namespace
}  // namespace attribution_reporting
