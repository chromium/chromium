// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_features.h"
#include "url/url_util.h"

namespace crosapi {

namespace gurl_os_handler_utils {

TEST(GurlOsHandlerUtilsTest, GetAshUrlFromLacrosUrl) {
  // To allow the "chrome" scheme, we need to add it to the registry.
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome", url::SCHEME_WITH_HOST);
  CHECK(GURL("chrome://version").has_host());

  EXPECT_EQ(GetAshUrlFromLacrosUrl(GURL("http://abc:123/def/ghi?jkl#mno")),
            GURL("http://abc:123/def/ghi?jkl#mno"));

  EXPECT_EQ(GetAshUrlFromLacrosUrl(GURL("chrome://abc/def/ghi?jkl#mno")),
            GURL("chrome://abc/def/ghi?jkl#mno"));

  EXPECT_EQ(GetAshUrlFromLacrosUrl(GURL("os://abc/def/ghi?jkl#mno")),
            GURL("chrome://abc/def/ghi?jkl#mno"));

  // os://settings is mapped to chrome://os-settings.
  EXPECT_EQ(GetAshUrlFromLacrosUrl(GURL("os://settings/def/ghi?jkl#mno")),
            GURL("chrome://os-settings/def/ghi?jkl#mno"));
}

class GurlOsHandlerUtilsParamTest : public ::testing::TestWithParam<bool> {
 public:
  GurlOsHandlerUtilsParamTest()
      : use_standard_compliant_non_special_scheme_url_parsing_(GetParam()) {
    scoped_feature_list_.InitWithFeatureState(
        url::kStandardCompliantNonSpecialSchemeURLParsing,
        use_standard_compliant_non_special_scheme_url_parsing_);
  }

 protected:
  bool use_standard_compliant_non_special_scheme_url_parsing_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(GurlOsHandlerUtilsParamTest, SanitizeAshUrl) {
  // To allow the "chrome" scheme, we need to add it to the registry.
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome", url::SCHEME_WITH_HOST);
  CHECK(GURL("chrome://version").has_host());

  // Invalid examples.
  if (use_standard_compliant_non_special_scheme_url_parsing_) {
    // Non-special URLs are supported when the feature is enabled.
    EXPECT_EQ(SanitizeAshUrl(GURL("os://version")), GURL("os://version"));
  } else {
    EXPECT_EQ(SanitizeAshUrl(GURL("os://version")), GURL());  // Unknown scheme.
  }
  EXPECT_EQ(SanitizeAshUrl(GURL("chrome://")), GURL());  // No host.

  // Valid examples.
  EXPECT_EQ(SanitizeAshUrl(GURL("chrome://version")), GURL("chrome://version"));
  EXPECT_EQ(SanitizeAshUrl(GURL("chrome://abc/def/ghi?jkl")),
            GURL("chrome://abc/def/ghi?jkl"));  // Query preserved.
  EXPECT_EQ(SanitizeAshUrl(GURL("chrome://abc:123/def/ghi?jkl#mno")),
            GURL("chrome://abc/def/ghi?jkl"));  // Port and ref removed.
  EXPECT_EQ(SanitizeAshUrl(GURL("https://abc:123/def/ghi?jkl#mno")),
            GURL("https://abc/def/ghi?jkl"));  // Here too.
}

INSTANTIATE_TEST_SUITE_P(All, GurlOsHandlerUtilsParamTest, ::testing::Bool());

TEST(GurlOsHandlerUtilsTest, IsAshUrlInList) {
  // To allow the "chrome" scheme, we need to add it to the registry.
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome", url::SCHEME_WITH_HOST);
  CHECK(GURL("chrome://version").has_host());

  std::vector<GURL> list_of_urls = {GURL("chrome://foo")};

  EXPECT_FALSE(IsAshUrlInList(GURL("os://version"), list_of_urls));
  EXPECT_FALSE(IsAshUrlInList(GURL("chrome://version"), list_of_urls));

  EXPECT_FALSE(IsAshUrlInList(GURL("os://foo"), list_of_urls));
  EXPECT_FALSE(IsAshUrlInList(GURL("chrome://fooz"), list_of_urls));

  EXPECT_TRUE(IsAshUrlInList(GURL("chrome://foo"), list_of_urls));
  EXPECT_TRUE(IsAshUrlInList(GURL("chrome://foo?baz"), list_of_urls));
  EXPECT_TRUE(IsAshUrlInList(GURL("chrome://foo#baz"), list_of_urls));
  EXPECT_TRUE(IsAshUrlInList(GURL("chrome://foo?bar#baz"), list_of_urls));
  EXPECT_TRUE(IsAshUrlInList(GURL("chrome://foo/bar"), list_of_urls));
  EXPECT_TRUE(IsAshUrlInList(GURL("chrome://foo/bar?baz"), list_of_urls));
  EXPECT_TRUE(IsAshUrlInList(GURL("chrome://foo/bar#baz"), list_of_urls));
  EXPECT_TRUE(IsAshUrlInList(GURL("chrome://foo/bar?baz#bax"), list_of_urls));
}

TEST(GurlOsHandlerUtilsTest, HasOsScheme) {
  EXPECT_TRUE(HasOsScheme(GURL("os://version?foo#bar")));
  EXPECT_TRUE(HasOsScheme(GURL("os://flags")));
  EXPECT_TRUE(HasOsScheme(GURL("OS://flags")));     // case insensitive.
  EXPECT_FALSE(HasOsScheme(GURL("os:/flags")));     // Proper '://' required.
  EXPECT_FALSE(HasOsScheme(GURL("os://")));         // There needs to be a host.
  EXPECT_FALSE(HasOsScheme(GURL("oo://version")));  // scheme need matching.
  EXPECT_FALSE(HasOsScheme(GURL("")));              // No crash.
  EXPECT_FALSE(HasOsScheme(GURL("::/bar")));        // No crash.
  EXPECT_FALSE(HasOsScheme(GURL("osos::/bar")));    // In string correct.
}

TEST(GurlOsHandlerUtilsTest, IsOsScheme) {
  EXPECT_TRUE(IsOsScheme("os"));
  EXPECT_TRUE(IsOsScheme("Os"));
  EXPECT_FALSE(IsOsScheme("oss"));
  EXPECT_FALSE(IsOsScheme(""));  // Should not crash.
}

}  // namespace gurl_os_handler_utils

}  // namespace crosapi
