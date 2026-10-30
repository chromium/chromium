// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/google_chrome_scheme_util.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace startup {

namespace {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr std::string_view kScheme = "google-chrome";
constexpr std::string_view kOtherScheme = "chromium";
#else
constexpr std::string_view kScheme = "chromium";
constexpr std::string_view kOtherScheme = "google-chrome";
#endif
}  // namespace

TEST(GoogleChromeSchemeUtilTest, StripGoogleChromeScheme) {
  base::test::ScopedFeatureList feature_list{features::kGoogleChromeScheme};

#if BUILDFLAG(IS_WIN)
  std::wstring scheme_prefix = base::ASCIIToWide(kScheme) + L"://";
  using StringType = std::wstring;
#else
  std::string scheme_prefix = base::StrCat({kScheme, "://"});
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
    std::string mixed_scheme_ascii(kScheme);
    mixed_scheme_ascii[0] = toupper(mixed_scheme_ascii[0]);
#if BUILDFLAG(IS_WIN)
    StringType mixed_scheme_prefix =
        base::ASCIIToWide(mixed_scheme_ascii) + L"://";
    StringType expected = L"example.com";
#else
    StringType mixed_scheme_prefix = base::StrCat({mixed_scheme_ascii, "://"});
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

  // Opaque and malformed separators supported
  {
    // google-chrome:example.com (missing //) - Now supported as opaque scheme.
#if BUILDFLAG(IS_WIN)
    StringType expected = L"example.com";
    StringType malformed = base::ASCIIToWide(kScheme) + L":" + expected;
#else
    StringType expected = "example.com";
    StringType malformed = base::StrCat({kScheme, ":", expected});
#endif
    base::FilePath::StringViewType arg = malformed;
    EXPECT_TRUE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, expected);
  }
  {
    // google-chrome:/example.com (missing one /) - Now supported as opaque
    // scheme.
#if BUILDFLAG(IS_WIN)
    StringType expected = L"/example.com";
    StringType malformed = base::ASCIIToWide(kScheme) + L":" + expected;
#else
    StringType expected = "/example.com";
    StringType malformed = base::StrCat({kScheme, ":", expected});
#endif
    base::FilePath::StringViewType arg = malformed;
    EXPECT_TRUE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, expected);
  }
  {
    // google-chrome:http://www.example.com (opaque http)
#if BUILDFLAG(IS_WIN)
    StringType expected = L"http://www.example.com";
    StringType opaque = base::ASCIIToWide(kScheme) + L":" + expected;
#else
    StringType expected = "http://www.example.com";
    StringType opaque = base::StrCat({kScheme, ":", expected});
#endif
    base::FilePath::StringViewType arg = opaque;
    EXPECT_TRUE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, expected);
  }
  {
    // google-chrome:file:///tmp/test (opaque file)
#if BUILDFLAG(IS_WIN)
    StringType expected = L"file:///tmp/test";
    StringType opaque = base::ASCIIToWide(kScheme) + L":" + expected;
#else
    StringType expected = "file:///tmp/test";
    StringType opaque = base::StrCat({kScheme, ":", expected});
#endif
    base::FilePath::StringViewType arg = opaque;
    EXPECT_TRUE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, expected);
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

TEST(GoogleChromeSchemeUtilTest, ExtractGoogleChromeSchemeInnerUrl) {
  base::test::ScopedFeatureList feature_list{features::kGoogleChromeScheme};

  // Standard case
  {
    GURL url(base::StrCat({kScheme, "://example.com"}));
    std::optional<GURL> result = ExtractGoogleChromeSchemeInnerUrl(url);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), GURL("example.com"));
  }

  // Opaque case
  {
    GURL url(base::StrCat({kScheme, ":http://example.com"}));
    std::optional<GURL> result = ExtractGoogleChromeSchemeInnerUrl(url);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), GURL("http://example.com"));
  }

  // File case
  {
    GURL url(base::StrCat({kScheme, ":file:///tmp/test"}));
    std::optional<GURL> result = ExtractGoogleChromeSchemeInnerUrl(url);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), GURL("file:///tmp/test"));
  }

  // No match
  {
    GURL url("http://example.com");
    std::optional<GURL> result = ExtractGoogleChromeSchemeInnerUrl(url);
    EXPECT_FALSE(result.has_value());
  }

  // Feature disabled
  {
    base::test::ScopedFeatureList disabled_feature;
    disabled_feature.InitAndDisableFeature(features::kGoogleChromeScheme);
    GURL url(base::StrCat({kScheme, "://example.com"}));
    std::optional<GURL> result = ExtractGoogleChromeSchemeInnerUrl(url);
    EXPECT_FALSE(result.has_value());
  }

  // Cross-scheme support (Strict mode - should fail)
  {
    GURL url(base::StrCat({kOtherScheme, "://example.com"}));
    std::optional<GURL> result = ExtractGoogleChromeSchemeInnerUrl(url);
    EXPECT_FALSE(result.has_value());
  }
}

}  // namespace startup
