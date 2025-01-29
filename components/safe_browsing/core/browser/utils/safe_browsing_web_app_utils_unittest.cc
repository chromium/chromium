// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/utils/safe_browsing_web_app_utils.h"

#include <optional>

#include "base/strings/string_util.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {
namespace {

struct GetSafeBrowsingWebAppKeyTestCase {
  const char* start_url;
  const char* manifest_id;
  const char* expected_start_url_origin = "";
  const char* expected_id_or_start_path = "";
};

TEST(SafeBrowsingWebAppUtilsTest, GetSafeBrowsingWebAppKey) {
  const GetSafeBrowsingWebAppKeyTestCase kBasicCases[] = {
      // Taking the id_or_start_path from the manifest_id.
      {
          .start_url = "https://example.test",
          .manifest_id = "https://example.test/path",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "path",
      },
      {
          .start_url = "https://example.test/index.html",
          .manifest_id = "https://example.test/path",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "path",
      },
      {
          .start_url = "https://example.test/path",
          .manifest_id = "https://example.test/path",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "path",
      },
      // Taking the id_or_start_path from the start_url because there is no
      // manifest_id.
      {
          .start_url = "https://example.test/path",
          .manifest_id = "",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "path",
      },
      // Leading slash is removed from the path.
      {
          .start_url = "https://example.test",
          .manifest_id = "",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "",
      },
      {
          .start_url = "https://example.test/",
          .manifest_id = "",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "",
      },
      {
          .start_url = "https://example.test",
          .manifest_id = "https://example.test",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "",
      },
      {
          .start_url = "https://example.test",
          .manifest_id = "https://example.test/",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "",
      },
      // Scheme can be non-https.
      {
          .start_url = "http://example.test/path",
          .manifest_id = "",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "path",
      },
      {
          .start_url = "http://example.test",
          .manifest_id = "http://example.test/path",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "path",
      },
      // Subdomains are kept.
      {
          .start_url = "http://www.sub.example.test/path",
          .manifest_id = "",
          .expected_start_url_origin = "www.sub.example.test",
          .expected_id_or_start_path = "path",
      },
      {
          .start_url = "http://www.sub.example.test",
          .manifest_id = "http://www.sub.example.test/path",
          .expected_start_url_origin = "www.sub.example.test",
          .expected_id_or_start_path = "path",
      },
      // Default port number is ignored.
      {
          .start_url = "https://example.test:443/path",
          .manifest_id = "",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "path",
      },
      {
          .start_url = "https://example.test",
          .manifest_id = "https://example.test:443/path",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "path",
      },
      // Non-default port number is included.
      {
          .start_url = "https://example.test:8888/path",
          .manifest_id = "",
          .expected_start_url_origin = "example.test:8888",
          .expected_id_or_start_path = "path",
      },
      {
          .start_url = "https://example.test:8888",
          .manifest_id = "https://example.test:8888/path",
          .expected_start_url_origin = "example.test:8888",
          .expected_id_or_start_path = "path",
      },
      // Fragment is removed.
      {
          .start_url = "https://example.test/path#heading",
          .manifest_id = "",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "path",
      },
      {
          .start_url = "https://example.test",
          .manifest_id = "https://example.test/path#heading",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "path",
      },
      // Username and password are ignored.
      {
          .start_url = "https://user:pass@example.test/path",
          .manifest_id = "",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "path",
      },
      {
          .start_url = "https://example.test",
          .manifest_id = "https://user:pass@example.test/path",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "path",
      },
      // Query params are retained.
      {
          .start_url = "https://example.test/path?hello=world",
          .manifest_id = "",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "path?hello=world",
      },
      {
          .start_url = "https://example.test",
          .manifest_id = "https://example.test/path?hello=world",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "path?hello=world",
      },
      // Characters are URL-escaped.
      {
          .start_url = "https://example.test/😀",
          .manifest_id = "",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "%F0%9F%98%80",
      },
      {
          .start_url = "https://example.test",
          .manifest_id = "https://example.test/😀",
          .expected_start_url_origin = "example.test",
          .expected_id_or_start_path = "%F0%9F%98%80",
      },
  };

  for (const auto& test_case : kBasicCases) {
    SCOPED_TRACE(
        base::JoinString({test_case.start_url, test_case.manifest_id}, ","));
    auto actual = GetSafeBrowsingWebAppKey(GURL(test_case.start_url),
                                           GURL(test_case.manifest_id));
    EXPECT_TRUE(actual.has_value());
    EXPECT_EQ(actual->start_url_origin(), test_case.expected_start_url_origin);
    EXPECT_EQ(actual->id_or_start_path(), test_case.expected_id_or_start_path);
  }
}

TEST(SafeBrowsingWebAppUtilsTest, GetSafeBrowsingWebAppKeyInvalid) {
  const GetSafeBrowsingWebAppKeyTestCase kInvalidCases[] = {
      // Invalid app origin.
      {
          .start_url = "",
          .manifest_id = "",
      },
      {
          .start_url = "",
          .manifest_id = "https://example.test",
      },
      // Different origins.
      {
          .start_url = "https://example.test",
          .manifest_id = "https://other.test",
      },
      // Different origins (schemes differ).
      {
          .start_url = "http://example.test",
          .manifest_id = "https://example.test",
      },
      // Path has length 0.
      {
          .start_url = "bogus://fake",
          .manifest_id = "",
      },
  };

  for (const auto& test_case : kInvalidCases) {
    SCOPED_TRACE(
        base::JoinString({test_case.start_url, test_case.manifest_id}, ","));
    EXPECT_EQ(GetSafeBrowsingWebAppKey(GURL(test_case.start_url),
                                       GURL(test_case.manifest_id)),
              std::nullopt);
  }
}

}  // namespace
}  // namespace safe_browsing
