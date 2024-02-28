// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/registrar.h"

#include <optional>
#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

TEST(RegistrarTest, ParseInfo) {
  const struct {
    const char* description;
    std::string_view header;
    base::expected<std::optional<Registrar>, PreferredPlatformError> expected;
  } kTestCases[] = {
      {
          "empty",
          "",
          std::nullopt,
      },
      {
          "list",
          R"("foo", "bar")",
          base::unexpected(PreferredPlatformError()),
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
          base::unexpected(PreferredPlatformError()),
      },
      {
          "unknown-value",
          "preferred-platform=abc",
          base::unexpected(PreferredPlatformError()),
      },
      {
          "invalid-type",
          "preferred-platform=\"os\"",
          base::unexpected(PreferredPlatformError()),
      },
      {
          "inner-list",
          "preferred-platform=(foo bar)",
          base::unexpected(PreferredPlatformError()),
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
