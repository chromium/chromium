// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/os_registration.h"

#include <vector>

#include "base/strings/string_piece.h"
#include "components/attribution_reporting/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace attribution_reporting {
namespace {

TEST(OsRegistration, ParseOsSourceOrTriggerHeader) {
  const struct {
    const char* description;
    base::StringPiece header;
    std::vector<OsRegistrationItem> expected;
  } kTestCases[] = {
      {
          "empty",
          "",
          {},
      },
      {
          "invalid_url",
          R"("foo")",
          {},
      },
      {
          "integer",
          "123",
          {},
      },
      {
          "token",
          "d",
          {},
      },
      {
          "byte_sequence",
          ":YWJj:",
          {},
      },
      {
          "valid_url_no_params",
          R"("https://d.test")",
          {OsRegistrationItem{.url = GURL("https://d.test")}},
      },
      {
          "extra_params_ignored",
          R"("https://d.test"; y=1)",
          {OsRegistrationItem{.url = GURL("https://d.test")}},
      },
      {
          "inner_list",
          R"(("https://d.test"))",
          {},
      },
      {
          "multiple",
          R"(123, "https://d.test", "", "https://e.test")",
          {
              OsRegistrationItem{.url = GURL("https://d.test")},
              OsRegistrationItem{.url = GURL("https://e.test")},
          },
      },
      {
          "debug_reporting_param",
          R"("https://d.test", "https://e.test";debug-reporting, "https://f.test";debug-reporting=?0)",
          {
              OsRegistrationItem{.url = GURL("https://d.test")},
              OsRegistrationItem{.url = GURL("https://e.test"),
                                 .debug_reporting = true},
              OsRegistrationItem{.url = GURL("https://f.test")},
          },
      },
      {
          "debug_reporting_param_wrong_type",
          R"("https://d.test"; debug-reporting=1)",
          {
              OsRegistrationItem{.url = GURL("https://d.test")},
          },
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
