// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/registrar_info.h"

#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "components/attribution_reporting/registrar.h"
#include "services/network/public/mojom/attribution.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::network::mojom::AttributionSupport;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;

TEST(RegistrarInfoTest, GetRegistrar) {
  const struct {
    const char* description;
    bool has_web_header;
    bool has_os_header;
    std::optional<Registrar> preferred_platform;
    AttributionSupport support;
    std::optional<Registrar> expected_registrar;
    IssueTypes expected_issues;
  } kTestCases[] = {
      {
          "no-headers",
          false,
          false,
          std::nullopt,
          AttributionSupport::kWeb,
          std::nullopt,
          IssueTypes(),
      },
      {
          "preferred-unspecified-both",
          true,
          true,
          std::nullopt,
          AttributionSupport::kWeb,
          std::nullopt,
          {IssueType::kWebAndOsHeaders},
      },
      {
          "preferred-unspecified-web",
          true,
          false,
          std::nullopt,
          AttributionSupport::kWeb,
          Registrar::kWeb,
          IssueTypes(),
      },
      {
          "preferred-unspecified-web-none",
          true,
          false,
          std::nullopt,
          AttributionSupport::kNone,
          std::nullopt,
          {IssueType::kWebIgnored},
      },
      {
          "preferred-unspecified-os",
          false,
          true,
          std::nullopt,
          AttributionSupport::kOs,
          Registrar::kOs,
          IssueTypes(),
      },
      {
          "preferred-unspecified-os-none",
          false,
          true,
          std::nullopt,
          AttributionSupport::kNone,
          std::nullopt,
          {IssueType::kOsIgnored},
      },
      {
          "preferred-os-both",
          true,
          true,
          Registrar::kOs,
          AttributionSupport::kWebAndOs,
          Registrar::kOs,
          IssueTypes(),
      },
      {
          "preferred-os-both-web",
          true,
          true,
          Registrar::kOs,
          AttributionSupport::kWeb,
          Registrar::kWeb,
          {IssueType::kOsIgnored},
      },
      {
          "preferred-os-both-none",
          true,
          true,
          Registrar::kOs,
          AttributionSupport::kNone,
          std::nullopt,
          {IssueType::kOsIgnored, IssueType::kWebIgnored},
      },
      {
          "preferred-os-os-web",
          false,
          true,
          Registrar::kOs,
          AttributionSupport::kWeb,
          std::nullopt,
          {IssueType::kOsIgnored},
      },
      {
          "preferred-os-web",
          true,
          false,
          Registrar::kOs,
          AttributionSupport::kWeb,
          std::nullopt,
          {IssueType::kNoOsHeader},
      },
      {
          "preferred-web-both",
          true,
          true,
          Registrar::kWeb,
          AttributionSupport::kWebAndOs,
          Registrar::kWeb,
          IssueTypes(),
      },
      {
          "preferred-web-both-os",
          true,
          true,
          Registrar::kWeb,
          AttributionSupport::kOs,
          Registrar::kOs,
          {IssueType::kWebIgnored},
      },
      {
          "preferred-web-both-none",
          true,
          true,
          Registrar::kWeb,
          AttributionSupport::kNone,
          std::nullopt,
          {IssueType::kWebIgnored, IssueType::kOsIgnored},
      },
      {
          "preferred-web-web-os",
          true,
          false,
          Registrar::kWeb,
          AttributionSupport::kOs,
          std::nullopt,
          {IssueType::kWebIgnored},
      },
      {
          "preferred-web-os",
          false,
          true,
          Registrar::kWeb,
          AttributionSupport::kOs,
          std::nullopt,
          {IssueType::kNoWebHeader},
      },
  };

  static const char kSourceRegistrationMetric[] =
      "Conversions.SourceRegistrationRegistrarIssue";
  static const char kTriggerRegistrationMetric[] =
      "Conversions.TriggerRegistrationRegistrarIssue";

  for (const bool is_source : {true, false}) {
    SCOPED_TRACE(is_source);

    const char* metric =
        is_source ? kSourceRegistrationMetric : kTriggerRegistrationMetric;

    for (const auto& test_case : kTestCases) {
      SCOPED_TRACE(test_case.description);

      base::HistogramTester histograms;

      RegistrarInfo expected;
      expected.registrar = test_case.expected_registrar;
      expected.issues = test_case.expected_issues;

      EXPECT_EQ(RegistrarInfo::Get(
                    test_case.has_web_header, test_case.has_os_header,
                    is_source, test_case.preferred_platform, test_case.support),
                expected);

      histograms.ExpectTotalCount(metric, test_case.expected_issues.size());
      for (IssueType issue : test_case.expected_issues) {
        histograms.ExpectBucketCount(metric, issue, 1);
      }
    }
  }
}

}  // namespace
}  // namespace attribution_reporting
