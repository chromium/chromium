// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/url_util.h"

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/ui/startup/google_chrome_scheme_util.h"
#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace startup {

TEST(UrlUtilTest, ValidateLaunchUrlWebUnsafe) {
  EXPECT_TRUE(ValidateLaunchUrlWebUnsafe(GURL("http://google.com")));
  EXPECT_TRUE(ValidateLaunchUrlWebUnsafe(GURL("https://google.com")));
  EXPECT_TRUE(ValidateLaunchUrlWebUnsafe(GURL("file:///tmp/test")));
  EXPECT_TRUE(ValidateLaunchUrlWebUnsafe(GURL("about:blank")));

  // chrome:// settings
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(ValidateLaunchUrlWebUnsafe(GURL("chrome://settings")));
#else
  EXPECT_FALSE(ValidateLaunchUrlWebUnsafe(GURL("chrome://settings")));
  // Approved settings subpage should be allowed on desktop platforms.
  EXPECT_TRUE(ValidateLaunchUrlWebUnsafe(
      GURL("chrome://settings/resetProfileSettings")));
#endif

  // javascript
  EXPECT_FALSE(ValidateLaunchUrlWebUnsafe(GURL("javascript:alert(1)")));

  // Invalid GURL
  EXPECT_FALSE(ValidateLaunchUrlWebUnsafe(GURL("")));
  EXPECT_FALSE(ValidateLaunchUrlWebUnsafe(GURL("https://")));

  // Non-blank about: URLs should be rejected
  EXPECT_FALSE(ValidateLaunchUrlWebUnsafe(GURL("about:settings")));
  EXPECT_FALSE(ValidateLaunchUrlWebUnsafe(GURL("about:about")));
  EXPECT_FALSE(ValidateLaunchUrlWebUnsafe(GURL("about:")));

#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(ValidateLaunchUrlWebUnsafe(
      GURL("content://packagename.providername/path")));
#else
  EXPECT_FALSE(ValidateLaunchUrlWebUnsafe(
      GURL("content://packagename.providername/path")));
#endif
}

TEST(UrlUtilTest, ValidateLaunchUrlWebSafe) {
  // Standard WebSafe urls should be accepted.
  EXPECT_TRUE(ValidateLaunchUrlWebSafe(GURL("http://google.com")));
  EXPECT_TRUE(ValidateLaunchUrlWebSafe(GURL("https://google.com")));
  EXPECT_TRUE(ValidateLaunchUrlWebSafe(GURL("about:blank")));

  // Privileged or local schemes should be rejected in WebSafe context.
  EXPECT_FALSE(ValidateLaunchUrlWebSafe(GURL("file:///tmp/test")));
  EXPECT_FALSE(ValidateLaunchUrlWebSafe(GURL("chrome://settings")));
  EXPECT_FALSE(
      ValidateLaunchUrlWebSafe(GURL("chrome://settings/resetProfileSettings")));
  EXPECT_FALSE(ValidateLaunchUrlWebSafe(GURL("javascript:alert(1)")));
  EXPECT_FALSE(ValidateLaunchUrlWebSafe(GURL("")));
  EXPECT_FALSE(ValidateLaunchUrlWebSafe(GURL("https://")));

  // Non-blank about: URLs should be rejected in WebSafe context.
  EXPECT_FALSE(ValidateLaunchUrlWebSafe(GURL("about:settings")));
  EXPECT_FALSE(ValidateLaunchUrlWebSafe(GURL("about:about")));
  EXPECT_FALSE(ValidateLaunchUrlWebSafe(GURL("about:")));
}

TEST(UrlUtilTest, ValidateUrlRejectsNestedSchemes) {
  // filesystem: nested schemes must be explicitly rejected in WebSafe context,
  // but are allowed/accepted in the WebUnsafe context to prevent command-line
  // regressions.
  EXPECT_FALSE(ValidateLaunchUrlWebSafe(
      GURL("filesystem:chrome://settings/temporary/t.txt")));
  EXPECT_FALSE(ValidateLaunchUrlWebSafe(
      GURL("filesystem:https://example.com/temporary/t.txt")));
  EXPECT_TRUE(ValidateLaunchUrlWebUnsafe(
      GURL("filesystem:chrome://settings/temporary/t.txt")));
  EXPECT_TRUE(ValidateLaunchUrlWebUnsafe(
      GURL("filesystem:https://example.com/temporary/t.txt")));

  // blob: nested schemes must be explicitly rejected in WebSafe context,
  // but are allowed/accepted in the WebUnsafe context to prevent command-line
  // regressions.
  EXPECT_FALSE(
      ValidateLaunchUrlWebSafe(GURL("blob:chrome://settings/guid-string")));
  EXPECT_FALSE(
      ValidateLaunchUrlWebSafe(GURL("blob:https://example.com/guid-string")));
  EXPECT_TRUE(
      ValidateLaunchUrlWebUnsafe(GURL("blob:chrome://settings/guid-string")));
  EXPECT_TRUE(
      ValidateLaunchUrlWebUnsafe(GURL("blob:https://example.com/guid-string")));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
TEST(UrlUtilTest, ValidateGoogleChromeSchemeUrls) {
  base::test::ScopedFeatureList feature_list{features::kGoogleChromeScheme};
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::string scheme = "google-chrome";
#else
  std::string scheme = "chromium";
#endif

  // 1. Laundering filesystem: URLs -> must be extracted and REJECTED by
  // WebSafe.
  {
    GURL url(scheme + ":filesystem:chrome://settings/temporary/t.txt");
    ASSERT_OK_AND_ASSIGN(GURL inner, ExtractGoogleChromeSchemeInnerUrl(url));
    EXPECT_FALSE(ValidateLaunchUrlWebSafe(inner));
  }

  // 2. Laundering blob: URLs -> must be extracted and REJECTED by WebSafe.
  {
    GURL url(scheme + ":blob:chrome://settings/guid-string");
    ASSERT_OK_AND_ASSIGN(GURL inner, ExtractGoogleChromeSchemeInnerUrl(url));
    EXPECT_FALSE(ValidateLaunchUrlWebSafe(inner));
  }

  // 3. Standard WebSafe http redirect -> must be extracted and APPROVED by
  // WebSafe.
  {
    GURL url(scheme + ":https://google.com");
    ASSERT_OK_AND_ASSIGN(GURL inner, ExtractGoogleChromeSchemeInnerUrl(url));
    EXPECT_TRUE(ValidateLaunchUrlWebSafe(inner));
  }

  // 4. Laundering chrome://version -> must be extracted and REJECTED by
  // WebSafe.
  {
    GURL url(scheme + ":chrome://version");
    ASSERT_OK_AND_ASSIGN(GURL inner, ExtractGoogleChromeSchemeInnerUrl(url));
    EXPECT_FALSE(ValidateLaunchUrlWebSafe(inner));
  }

  // 5. Laundering chrome://settings -> must be extracted and REJECTED by
  // WebSafe.
  {
    GURL url(scheme + ":chrome://settings");
    ASSERT_OK_AND_ASSIGN(GURL inner, ExtractGoogleChromeSchemeInnerUrl(url));
    EXPECT_FALSE(ValidateLaunchUrlWebSafe(inner));
  }
}
#endif

}  // namespace startup
