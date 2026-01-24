// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/google_chrome_scheme_util.h"

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace startup {

namespace {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kScheme[] = "google-chrome";
#else
const char kScheme[] = "chromium";
#endif
}  // namespace

TEST(GoogleChromeSchemeUtilTest, StripGoogleChromeScheme) {
  base::test::ScopedFeatureList feature_list{features::kGoogleChromeScheme};

#if BUILDFLAG(IS_WIN)
  std::wstring scheme_prefix = base::ASCIIToWide(kScheme) + L"://";
  using StringType = std::wstring;
#else
  std::string scheme_prefix = std::string(kScheme) + "://";
  using StringType = std::string;
#endif

  // Match
  {
    StringType expected =
#if BUILDFLAG(IS_WIN)
        L"example.com";
#else
        "example.com";
#endif
    StringType arg_str = scheme_prefix + expected;
    base::FilePath::StringViewType arg = arg_str;
    EXPECT_TRUE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, expected);
  }

  // No match (http)
  {
    StringType arg_str =
#if BUILDFLAG(IS_WIN)
        L"http://example.com";
#else
        "http://example.com";
#endif
    base::FilePath::StringViewType arg = arg_str;
    EXPECT_FALSE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, arg_str);
  }

  // Case insensitive
  {
    // Construct mixed case scheme.
    std::string mixed_scheme_ascii = kScheme;
    mixed_scheme_ascii[0] = toupper(mixed_scheme_ascii[0]);
#if BUILDFLAG(IS_WIN)
    StringType mixed_scheme_prefix =
        base::ASCIIToWide(mixed_scheme_ascii) + L"://";
    StringType expected = L"example.com";
#else
    StringType mixed_scheme_prefix = mixed_scheme_ascii + "://";
    StringType expected = "example.com";
#endif
    StringType arg_str = mixed_scheme_prefix + expected;
    base::FilePath::StringViewType arg = arg_str;
    EXPECT_TRUE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, expected);
  }

  // Empty payload
  {
    StringType arg_str = scheme_prefix;
    base::FilePath::StringViewType arg = arg_str;
    EXPECT_TRUE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, StringType());
  }

  // Malformed separators
  {
    // google-chrome:example.com (missing //)
#if BUILDFLAG(IS_WIN)
    StringType malformed = base::ASCIIToWide(kScheme) + L":example.com";
#else
    StringType malformed = std::string(kScheme) + ":example.com";
#endif
    base::FilePath::StringViewType arg = malformed;
    EXPECT_FALSE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, malformed);
  }
  {
    // google-chrome:/example.com (missing one /)
#if BUILDFLAG(IS_WIN)
    StringType malformed = base::ASCIIToWide(kScheme) + L":/example.com";
#else
    StringType malformed = std::string(kScheme) + ":/example.com";
#endif
    base::FilePath::StringViewType arg = malformed;
    EXPECT_FALSE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, malformed);
  }

  // Feature disabled
  {
    base::test::ScopedFeatureList disabled_feature;
    disabled_feature.InitAndDisableFeature(features::kGoogleChromeScheme);

    StringType expected =
#if BUILDFLAG(IS_WIN)
        L"example.com";
#else
        "example.com";
#endif
    StringType arg_str = scheme_prefix + expected;
    base::FilePath::StringViewType arg = arg_str;
    // Should NOT strip if feature disabled.
    EXPECT_FALSE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, arg_str);
  }
}

TEST(GoogleChromeSchemeUtilTest, ValidateUrl) {
  EXPECT_TRUE(ValidateUrl(GURL("http://google.com")));
  EXPECT_TRUE(ValidateUrl(GURL("https://google.com")));
  EXPECT_TRUE(ValidateUrl(GURL("file:///tmp/test")));
  EXPECT_TRUE(ValidateUrl(GURL("about:blank")));

  // chrome:// settings
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(ValidateUrl(GURL("chrome://settings")));
#else
  EXPECT_FALSE(ValidateUrl(GURL("chrome://settings")));
#endif

  // javascript
  EXPECT_FALSE(ValidateUrl(GURL("javascript:alert(1)")));

  // Invalid GURL
  EXPECT_FALSE(ValidateUrl(GURL("")));
}

}  // namespace startup
