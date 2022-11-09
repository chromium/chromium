// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/os_registration.h"

#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace attribution_reporting {
namespace {

template <typename T>
void TestParse() {
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
          "url_not_in_http_family",
          R"("wss://d.test")",
          GURL(),
      },
      {
          "url_not_potentially_trustworthy",
          R"("http://d.test")",
          GURL(),
      },
      {
          "extra_params_ignored",
          R"("https://d.test"; y=1)",
          GURL("https://d.test"),
      },
  };

  for (const auto& test_case : kTestCases) {
    absl::optional<T> actual = T::Parse(test_case.header);

    EXPECT_EQ(test_case.expected.is_valid(), actual.has_value())
        << test_case.description;

    if (test_case.expected.is_valid())
      EXPECT_EQ(test_case.expected, actual->url()) << test_case.description;
  }
}

TEST(OsSource, Parse) {
  TestParse<OsSource>();
}

TEST(OsTrigger, Parse) {
  TestParse<OsTrigger>();
}

}  // namespace
}  // namespace attribution_reporting
