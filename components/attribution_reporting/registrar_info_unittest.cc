// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/registrar_info.h"

#include <optional>

#include "components/attribution_reporting/registrar.h"
#include "services/network/public/mojom/attribution.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::network::mojom::AttributionSupport;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;

TEST(RegistrarInfoTest, GetForSource) {
  const struct {
    const char* description;
    bool has_web_header;
    bool has_os_header;
    std::optional<Registrar> preferred_platform;
    AttributionSupport support;
    ::testing::Matcher<RegistrarInfo> matches;
  } kTestCases[] = {
      {
          "no-headers",
          false,
          false,
          std::nullopt,
          AttributionSupport::kWeb,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues, IssueTypes())),
      },
      {
          "preferred-unspecified-both",
          true,
          true,
          std::nullopt,
          AttributionSupport::kWeb,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kWebAndOsHeaders}))),
      },
      {
          "preferred-unspecified-web",
          true,
          false,
          std::nullopt,
          AttributionSupport::kWeb,
          AllOf(Field(&RegistrarInfo::registrar, Registrar::kWeb),
                Field(&RegistrarInfo::issues, IssueTypes())),
      },
      {
          "preferred-unspecified-web-none",
          true,
          false,
          std::nullopt,
          AttributionSupport::kNone,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kSourceIgnored}))),
      },
      {
          "preferred-unspecified-os",
          false,
          true,
          std::nullopt,
          AttributionSupport::kOs,
          AllOf(Field(&RegistrarInfo::registrar, Registrar::kOs),
                Field(&RegistrarInfo::issues, IssueTypes())),
      },
      {
          "preferred-unspecified-os-none",
          false,
          true,
          std::nullopt,
          AttributionSupport::kNone,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kOsSourceIgnored}))),
      },
      {
          "preferred-os-both",
          true,
          true,
          Registrar::kOs,
          AttributionSupport::kWebAndOs,
          AllOf(Field(&RegistrarInfo::registrar, Registrar::kOs),
                Field(&RegistrarInfo::issues, IssueTypes())),
      },
      {
          "preferred-os-both-web",
          true,
          true,
          Registrar::kOs,
          AttributionSupport::kWeb,
          AllOf(Field(&RegistrarInfo::registrar, Registrar::kWeb),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kOsSourceIgnored}))),
      },
      {
          "preferred-os-both-none",
          true,
          true,
          Registrar::kOs,
          AttributionSupport::kNone,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kOsSourceIgnored,
                                  IssueType::kSourceIgnored}))),
      },
      {
          "preferred-os-os-web",
          false,
          true,
          Registrar::kOs,
          AttributionSupport::kWeb,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kOsSourceIgnored}))),
      },
      {
          "preferred-os-web",
          true,
          false,
          Registrar::kOs,
          AttributionSupport::kWeb,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kNoRegisterOsSourceHeader}))),
      },
      {
          "preferred-web-both",
          true,
          true,
          Registrar::kWeb,
          AttributionSupport::kWebAndOs,
          AllOf(Field(&RegistrarInfo::registrar, Registrar::kWeb),
                Field(&RegistrarInfo::issues, IssueTypes())),
      },
      {
          "preferred-web-both-os",
          true,
          true,
          Registrar::kWeb,
          AttributionSupport::kOs,
          AllOf(Field(&RegistrarInfo::registrar, Registrar::kOs),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kSourceIgnored}))),
      },
      {
          "preferred-web-both-none",
          true,
          true,
          Registrar::kWeb,
          AttributionSupport::kNone,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kSourceIgnored,
                                  IssueType::kOsSourceIgnored}))),
      },
      {
          "preferred-web-web-os",
          true,
          false,
          Registrar::kWeb,
          AttributionSupport::kOs,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kSourceIgnored}))),
      },
      {
          "preferred-web-os",
          false,
          true,
          Registrar::kWeb,
          AttributionSupport::kOs,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kNoRegisterSourceHeader}))),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    EXPECT_THAT(
        RegistrarInfo::Get(test_case.has_web_header, test_case.has_os_header,
                              /*is_source=*/true, test_case.preferred_platform,
                              test_case.support),
        test_case.matches);
  }
}

TEST(RegistrarInfoTest, GetForTrigger) {
  const struct {
    const char* description;
    bool has_web_header;
    bool has_os_header;
    std::optional<Registrar> preferred_platform;
    AttributionSupport support;
    ::testing::Matcher<RegistrarInfo> matches;
  } kTestCases[] = {
      {
          "no-headers",
          false,
          false,
          std::nullopt,
          AttributionSupport::kWeb,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues, IssueTypes())),
      },
      {
          "preferred-unspecified-both",
          true,
          true,
          std::nullopt,
          AttributionSupport::kWeb,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kWebAndOsHeaders}))),
      },
      {
          "preferred-unspecified-web",
          true,
          false,
          std::nullopt,
          AttributionSupport::kWeb,
          AllOf(Field(&RegistrarInfo::registrar, Registrar::kWeb),
                Field(&RegistrarInfo::issues, IssueTypes())),
      },
      {
          "preferred-unspecified-web-none",
          true,
          false,
          std::nullopt,
          AttributionSupport::kNone,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kTriggerIgnored}))),
      },
      {
          "preferred-unspecified-os",
          false,
          true,
          std::nullopt,
          AttributionSupport::kOs,
          AllOf(Field(&RegistrarInfo::registrar, Registrar::kOs),
                Field(&RegistrarInfo::issues, IssueTypes())),
      },
      {
          "preferred-unspecified-os-none",
          false,
          true,
          std::nullopt,
          AttributionSupport::kNone,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kOsTriggerIgnored}))),
      },
      {
          "preferred-os-both",
          true,
          true,
          Registrar::kOs,
          AttributionSupport::kWebAndOs,
          AllOf(Field(&RegistrarInfo::registrar, Registrar::kOs),
                Field(&RegistrarInfo::issues, IssueTypes())),
      },
      {
          "preferred-os-both-web",
          true,
          true,
          Registrar::kOs,
          AttributionSupport::kWeb,
          AllOf(Field(&RegistrarInfo::registrar, Registrar::kWeb),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kOsTriggerIgnored}))),
      },
      {
          "preferred-os-both-none",
          true,
          true,
          Registrar::kOs,
          AttributionSupport::kNone,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kOsTriggerIgnored,
                                  IssueType::kTriggerIgnored}))),
      },
      {
          "preferred-os-os-web",
          false,
          true,
          Registrar::kOs,
          AttributionSupport::kWeb,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kOsTriggerIgnored}))),
      },
      {
          "preferred-os-web",
          true,
          false,
          Registrar::kOs,
          AttributionSupport::kWeb,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kNoRegisterOsTriggerHeader}))),
      },
      {
          "preferred-web-both",
          true,
          true,
          Registrar::kWeb,
          AttributionSupport::kWebAndOs,
          AllOf(Field(&RegistrarInfo::registrar, Registrar::kWeb),
                Field(&RegistrarInfo::issues, IssueTypes())),
      },
      {
          "preferred-web-both-os",
          true,
          true,
          Registrar::kWeb,
          AttributionSupport::kOs,
          AllOf(Field(&RegistrarInfo::registrar, Registrar::kOs),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kTriggerIgnored}))),
      },
      {
          "preferred-web-both-none",
          true,
          true,
          Registrar::kWeb,
          AttributionSupport::kNone,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kTriggerIgnored,
                                  IssueType::kOsTriggerIgnored}))),
      },
      {
          "preferred-web-web-os",
          true,
          false,
          Registrar::kWeb,
          AttributionSupport::kOs,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kTriggerIgnored}))),
      },
      {
          "preferred-web-os",
          false,
          true,
          Registrar::kWeb,
          AttributionSupport::kOs,
          AllOf(Field(&RegistrarInfo::registrar, Eq(std::nullopt)),
                Field(&RegistrarInfo::issues,
                      IssueTypes({IssueType::kNoRegisterTriggerHeader}))),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    EXPECT_THAT(
        RegistrarInfo::Get(test_case.has_web_header, test_case.has_os_header,
                              /*is_source=*/false, test_case.preferred_platform,
                              test_case.support),
        test_case.matches);
  }
}

}  // namespace
}  // namespace attribution_reporting
