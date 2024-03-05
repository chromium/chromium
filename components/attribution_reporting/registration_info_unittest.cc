// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/registration_info.h"

#include <optional>
#include <string_view>

#include "components/attribution_reporting/registrar.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

TEST(RegistrationInfoTest, ParseInfo) {
  const struct {
    const char* description;
    std::string_view header;
    base::expected<RegistrationInfo, RegistrationInfoError>
        expected_cross_app_web_enabled;
    base::expected<RegistrationInfo, RegistrationInfoError>
        expected_cross_app_web_disabled;
  } kTestCases[] = {
      {
          "empty",
          "",
          RegistrationInfo(),
          RegistrationInfo(),
      },
      {
          "all-fields",
          "preferred-platform=os,report-header-errors",
          RegistrationInfo{
              .preferred_platform = Registrar::kOs,
              .report_header_errors = true,
          },
          RegistrationInfo{
              .report_header_errors = true,
          },
      },
      {
          "list",
          R"("foo", "bar")",
          base::unexpected(RegistrationInfoError()),
          base::unexpected(RegistrationInfoError()),
      },
      {
          "prefer-web",
          "preferred-platform=web",
          RegistrationInfo{
              .preferred_platform = Registrar::kWeb,
          },
          RegistrationInfo(),
      },
      {
          "prefer-os",
          "preferred-platform=os",
          RegistrationInfo{
              .preferred_platform = Registrar::kOs,
          },
          RegistrationInfo(),
      },
      {
          "preferred-platform-parameter-ignored",
          "preferred-platform=os;abc",
          RegistrationInfo{
              .preferred_platform = Registrar::kOs,
          },
          RegistrationInfo(),
      },
      {
          "preferred-platform-missing-value",
          "preferred-platform",
          base::unexpected(RegistrationInfoError()),
          RegistrationInfo(),
      },
      {
          "preferred-platform-unknown-value",
          "preferred-platform=abc",
          base::unexpected(RegistrationInfoError()),
          RegistrationInfo(),
      },
      {
          "preferred-platform-invalid-type",
          "preferred-platform=\"os\"",
          base::unexpected(RegistrationInfoError()),
          RegistrationInfo(),
      },
      {
          "preferred-platform-inner-list",
          "preferred-platform=(foo bar)",
          base::unexpected(RegistrationInfoError()),
          RegistrationInfo(),
      },
      {
          "unknown-field",
          "unknown",
          RegistrationInfo(),
          RegistrationInfo(),
      },
      {
          "report-header-errors",
          "report-header-errors",
          RegistrationInfo{
              .report_header_errors = true,
          },
          RegistrationInfo{
              .report_header_errors = true,
          },
      },
      {
          "report-header-errors-parameter-ignored",
          "report-header-errors=?0;abc",
          RegistrationInfo{
              .report_header_errors = false,
          },
          RegistrationInfo{
              .report_header_errors = false,
          },
      },
      {
          "report-header-errors-invalid-type",
          "report-header-errors=abc",
          base::unexpected(RegistrationInfoError()),
          base::unexpected(RegistrationInfoError()),
      },
      {
          "report-header-errors-inner-list",
          "report-header-errors=(foo bar)",
          base::unexpected(RegistrationInfoError()),
          base::unexpected(RegistrationInfoError()),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    EXPECT_EQ(RegistrationInfo::ParseInfo(test_case.header,
                                          /*cross_app_web_enabled=*/true),
              test_case.expected_cross_app_web_enabled);
    EXPECT_EQ(RegistrationInfo::ParseInfo(test_case.header,
                                          /*cross_app_web_enabled=*/false),
              test_case.expected_cross_app_web_disabled);
  }
}

}  // namespace
}  // namespace attribution_reporting
