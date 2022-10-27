// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/parse.h"

#include <string>

#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace attribution_reporting {

namespace {

TEST(AttributionOsParsing, OsSourceRegistrationHeader) {
  const struct {
    const char* description;
    base::StringPiece header;
    absl::optional<OsSource> expected;
  } kTestCases[] = {
      {
          "empty",
          base::StringPiece(),
          absl::nullopt,
      },
      {
          "invalid_url",
          R"("foo")",
          absl::nullopt,
      },
      {
          "not_string",
          "123",
          absl::nullopt,
      },
      {
          "valid_url_no_params",
          R"("https://d.test")",
          absl::nullopt,
      },
      {
          "valid_url_missing_web_dest",
          R"("https://d.test"; os-destination="com.d.app")",
          absl::nullopt,
      },
      {
          "valid_url_missing_os_dest",
          R"(https://d.test"; web-destination="https://e.test")",
          absl::nullopt,
      },
      {
          "valid_url_web_dest_wrong_type",
          R"("https://d.test"; web-destination=1; os-destination="com.d.app")",
          absl::nullopt,
      },
      {
          "valid_url_os_dest_wrong_type",
          R"("https://d.test"; web-destination="https://e.test"; os-destination=1)",
          absl::nullopt,
      },
      {
          "valid_url_invalid_web_dest",
          R"("https://d.test"; web-destination="e.test"; os-destination="com.d.app")",
          absl::nullopt,
      },
      {
          "valid_header",
          R"("https://d.test"; web-destination="https://e.test"; os-destination="com.d.app")",
          *OsSource::Create(
              /*url=*/GURL("https://d.test"),
              /*os_destination=*/"com.d.app",
              /*web_destination=*/url::Origin::Create(GURL("https://e.test"))),
      },
      {
          "url_not_in_http_family",
          R"("wss://d.test"; web-destination="https://e.test"; os-destination="com.d.app")",
          absl::nullopt,
      },
      {
          "url_not_potentially_trustworthy",
          R"("http://d.test"; web-destination="https://e.test"; os-destination="com.d.app")",
          absl::nullopt,
      },
      {
          "extra_params_ignored",
          R"("https://d.test"; web-destination="https://e.test"; os-destination="com.d.app"; y=1)",
          *OsSource::Create(
              /*url=*/GURL("https://d.test"),
              /*os_destination=*/"com.d.app",
              /*web_destination=*/
              url::Origin::Create(GURL("https://e.test"))),
      },
      {
          "web_dest_full_url",
          R"("https://d.test"; web-destination="https://e.test/x"; os-destination="com.d.app"; y=1)",
          *OsSource::Create(
              /*url=*/GURL("https://d.test"),
              /*os_destination=*/"com.d.app",
              /*web_destination=*/
              url::Origin::Create(GURL("https://e.test"))),
      },
  };

  for (const auto& test_case : kTestCases) {
    auto actual = OsSource::Parse(test_case.header);
    if (test_case.expected) {
      EXPECT_EQ(actual->url(), test_case.expected->url())
          << test_case.description;
      EXPECT_EQ(actual->os_destination(), test_case.expected->os_destination())
          << test_case.description;
      EXPECT_EQ(actual->web_destination(),
                test_case.expected->web_destination())
          << test_case.description;
    } else {
      EXPECT_FALSE(actual) << test_case.description;
    }
  }
}

TEST(AttributionOsParsing, OsTriggerRegistrationHeader) {
  const struct {
    const char* description;
    base::StringPiece header;
    absl::optional<OsTrigger> expected;
  } kTestCases[] = {
      {
          "empty",
          base::StringPiece(),
          absl::nullopt,
      },
      {
          "invalid_url",
          R"("foo")",
          absl::nullopt,
      },
      {
          "not_string",
          "123",
          absl::nullopt,
      },
      {
          "url_not_in_http_family",
          R"("wss://d.test")",
          absl::nullopt,
      },
      {
          "url_not_potentially_trustworthy",
          R"("http://d.test")",
          absl::nullopt,
      },
      {
          "valid_url",
          R"("https://d.test")",
          *OsTrigger::Create(GURL("https://d.test")),
      },
      {
          "params_ignored",
          R"("https://d.test"; x=1)",
          *OsTrigger::Create(GURL("https://d.test")),
      },
  };

  for (const auto& test_case : kTestCases) {
    auto actual = OsTrigger::Parse(test_case.header);
    if (test_case.expected) {
      EXPECT_EQ(actual->url(), test_case.expected->url())
          << test_case.description;
    } else {
      EXPECT_FALSE(actual) << test_case.description;
    }
  }
}

}  // namespace

}  // namespace attribution_reporting
