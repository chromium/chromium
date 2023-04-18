// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/os_registration.h"

#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace attribution_reporting {
namespace {

TEST(OsRegistration, ParseOsSourceOrTriggerHeader) {
  const struct {
    const char* description;
    base::StringPiece header;
    GURL expected;
  } kTestCases[] = {
      {
          "empty",
          "",
          GURL(),
      },
      {
          "invalid_url",
          R"("foo")",
          GURL(),
      },
      {
          "not_string",
          "123",
          GURL(),
      },
      {
          "valid_url_no_params",
          R"("https://d.test")",
          GURL("https://d.test"),
      },
      {
          "extra_params_ignored",
          R"("https://d.test"; y=1)",
          GURL("https://d.test"),
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(ParseOsSourceOrTriggerHeader(test_case.header),
              test_case.expected)
        << test_case.description;
  }
}

}  // namespace
}  // namespace attribution_reporting
