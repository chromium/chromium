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
    base::expected<std::optional<Registrar>, RegistrationInfoError> expected;
  } kTestCases[] = {
      {
          "empty",
          "",
          std::nullopt,
      },
      {
          "list",
          R"("foo", "bar")",
          base::unexpected(RegistrationInfoError()),
      },
      {
          "prefer-web",
          "preferred-platform=web",
          Registrar::kWeb,
      },
      {
          "prefer-os",
          "preferred-platform=os",
          Registrar::kOs,
      },
      {
          "parameter-ignored",
          "preferred-platform=os;abc",
          Registrar::kOs,
      },
      {
          "missing-value",
          "preferred-platform",
          base::unexpected(RegistrationInfoError()),
      },
      {
          "unknown-value",
          "preferred-platform=abc",
          base::unexpected(RegistrationInfoError()),
      },
      {
          "invalid-type",
          "preferred-platform=\"os\"",
          base::unexpected(RegistrationInfoError()),
      },
      {
          "inner-list",
          "preferred-platform=(foo bar)",
          base::unexpected(RegistrationInfoError()),
      },
      {
          "unknown-field",
          "unknown",
          std::nullopt,
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    EXPECT_EQ(ParseInfo(test_case.header), test_case.expected);
  }
}

}  // namespace
}  // namespace attribution_reporting
