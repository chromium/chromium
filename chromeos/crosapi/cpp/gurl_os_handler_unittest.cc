// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace crosapi {

namespace gurl_os_handler_utils {

TEST(GurlOsHandlerUtilsTest, SanitizeAshURL) {
  // Using a known GURL scheme, we should get scheme + host + sub-host.
  EXPECT_EQ(SanitizeAshURL(GURL("http://version")), GURL("http://version"));
  EXPECT_EQ(SanitizeAshURL(GURL("http://version/#foo")),
            GURL("http://version"));
  EXPECT_EQ(SanitizeAshURL(GURL("http://version/1/#foo")),
            GURL("http://version/1/"));
  EXPECT_EQ(SanitizeAshURL(GURL("http://version/1/?foo")),
            GURL("http://version/1/"));
  EXPECT_EQ(SanitizeAshURL(GURL("http://version/foo/?bar")),
            GURL("http://version/foo/"));
  EXPECT_EQ(SanitizeAshURL(GURL("http://version/foo/?bar"),
                           /*include_path=*/false),
            GURL("http://version/"));
  EXPECT_EQ(SanitizeAshURL(GURL("http://version/foo")),
            GURL("http://version/foo"));

  // Using an unknown GURL scheme, we should get the same.
  EXPECT_EQ(SanitizeAshURL(GURL("os://version")), GURL("os://version"));
  EXPECT_EQ(SanitizeAshURL(GURL("os://version/#foo")), GURL("os://version"));
  EXPECT_EQ(SanitizeAshURL(GURL("os://version/1/#foo")),
            GURL("os://version/1"));
  EXPECT_EQ(SanitizeAshURL(GURL("os://version/1/?foo")),
            GURL("os://version/1"));
  EXPECT_EQ(SanitizeAshURL(GURL("os://version/foo/?bar")),
            GURL("os://version/foo"));

  // Using a standard path, we should get scheme + host.
  EXPECT_EQ(SanitizeAshURL(GURL("os://version")), GURL("os://version"));

  // Passing in an empty scheme will lead into an invalid result.
  EXPECT_EQ(SanitizeAshURL(GURL("os://")), GURL(""));

  // Any path more than that will be ignored.
  EXPECT_EQ(SanitizeAshURL(GURL("os://version/1")), GURL("os://version/1"));
  EXPECT_EQ(SanitizeAshURL(GURL("os://version/1/foo")), GURL("os://version/1"));
  EXPECT_EQ(SanitizeAshURL(GURL("os://version#")), GURL("os://version"));
  EXPECT_EQ(SanitizeAshURL(GURL("os://version?")), GURL("os://version"));
  EXPECT_EQ(SanitizeAshURL(GURL("os://version&")), GURL("os://version"));

  // Special characters get ignored
  EXPECT_EQ(SanitizeAshURL(GURL("os://version%65")), GURL("os://version"));

  // Passing in any parameters, etc. we should get nothing as that is invalid.
  EXPECT_EQ(SanitizeAshURL(GURL("os://version/query?foo=1&bar=1")),
            GURL("os://version/query"));
  EXPECT_EQ(SanitizeAshURL(GURL("os://version/+/query")), GURL("os://version"));
  EXPECT_EQ(SanitizeAshURL(GURL("os://version/#foo")), GURL("os://version"));
  EXPECT_EQ(SanitizeAshURL(GURL("os://version/1/#foo")),
            GURL("os://version/1"));
  EXPECT_EQ(SanitizeAshURL(GURL("os://version/1/?foo")),
            GURL("os://version/1"));
  EXPECT_EQ(SanitizeAshURL(GURL("os://version/1/?foo"), /*include_path=*/false),
            GURL("os://version"));

  // Invalid syntax of kind will be detected by GURL as well.
  EXPECT_EQ(SanitizeAshURL(GURL("os://version/foo#")),
            GURL("os://version/foo"));
  EXPECT_EQ(SanitizeAshURL(GURL("os://version/ver\\")),
            GURL("os://version/ver"));
  EXPECT_EQ(SanitizeAshURL(GURL("os://version/foo bar")),
            GURL("os://version/foo"));

  // Case insensitive
  EXPECT_EQ(SanitizeAshURL(GURL("Os://Foo/Bar")), GURL("os://foo/bar"));

  // Check that a port get removed.
  EXPECT_EQ(SanitizeAshURL(GURL("https://a.test:35649/title1.html")),
            GURL("https://a.test/title1.html"));
}

TEST(GurlOsHandlerUtilsTest, IsURLInList) {
  std::vector<GURL> list_of_urls = {
      GURL("os://version"),  GURL("Os://version2"), GURL("http://version"),
      GURL("http://Foobar"), GURL("os://flags"),    GURL("os://foo/bar")};
  // As we expect the input to be sanitized, we cannot add any parameters.
  EXPECT_TRUE(IsUrlInList(GURL("os://version"), list_of_urls));
  EXPECT_TRUE(IsUrlInList(GURL("os://flags"), list_of_urls));
  EXPECT_TRUE(IsUrlInList(GURL("http://version"), list_of_urls));
  // Sub hosts can - or cannot be supplied.
  EXPECT_TRUE(IsUrlInList(GURL("os://version/12"), list_of_urls));
  // Does not exist.
  EXPECT_FALSE(IsUrlInList(GURL("http://flags"), list_of_urls));
  // Our internal URLs will be treated part in part in/sensitive. The scheme
  // is treated insensitive, while the host is not - so if an os:// URL in the
  // list is upper case, it cannot be (ever) found.
  // Note that DCHECKs make sure that no case insensitive host can be passed.
  EXPECT_TRUE(IsUrlInList(GURL("Os://version"), list_of_urls));
  EXPECT_TRUE(IsUrlInList(GURL("Os://version2"), list_of_urls));
  // Whereas - if there is a valid scheme, the rest of the host will be handled
  // case insensitive and be found.
  EXPECT_TRUE(IsUrlInList(GURL("http://fOOBar"), list_of_urls));
  // Sub hosts should also be handled.
  EXPECT_TRUE(IsUrlInList(GURL("os://foo/bar"), list_of_urls));
  EXPECT_FALSE(IsUrlInList(GURL("os://foo/ba"), list_of_urls));
}

TEST(GurlOsHandlerUtilsTest, IsAshOsUrl) {
  // As we expect the input to be sanitized, we cannot add any parameters.
  EXPECT_TRUE(IsAshOsUrl(GURL("os://version")));
  EXPECT_TRUE(IsAshOsUrl(GURL("os://flags")));
  EXPECT_TRUE(IsAshOsUrl(GURL("OS://flags")));     // case insensitive.
  EXPECT_FALSE(IsAshOsUrl(GURL("os:/flags")));     // Proper '://' required.
  EXPECT_FALSE(IsAshOsUrl(GURL("os://")));         // There needs to be a host.
  EXPECT_FALSE(IsAshOsUrl(GURL("oo://version")));  // scheme need matching.
  EXPECT_FALSE(IsAshOsUrl(GURL("")));              // No crash.
  EXPECT_FALSE(IsAshOsUrl(GURL("::/bar")));        // No crash.
  EXPECT_FALSE(IsAshOsUrl(GURL("osos::/bar")));    // In string correct.
}

TEST(GurlOsHandlerUtilsTest, IsAshOsAsciiScheme) {
  // As we expect the input to be sanitized, we cannot add any parameters.
  EXPECT_TRUE(IsAshOsAsciiScheme("os"));
  EXPECT_TRUE(IsAshOsAsciiScheme("Os"));
  EXPECT_FALSE(IsAshOsAsciiScheme(""));  // Should not crash.
  EXPECT_FALSE(IsAshOsAsciiScheme("so"));
  EXPECT_FALSE(IsAshOsAsciiScheme("soo"));
}

TEST(GurlOsHandlerUtilsTest, AshOsUrlHost) {
  EXPECT_EQ(AshOsUrlHost(GURL("os://flags")), "flags");
  EXPECT_EQ(AshOsUrlHost(GURL("os://flags/test")), "flags/test");
  EXPECT_EQ(AshOsUrlHost(GURL("os://flags?foo::bar")), "flags");
  EXPECT_EQ(AshOsUrlHost(GURL("os://flags#foo")), "flags");
  EXPECT_EQ(AshOsUrlHost(GURL("os://flags/foo")), "flags/foo");
  EXPECT_EQ(AshOsUrlHost(GURL("os://FlagS/foo")), "flags/foo");
  EXPECT_EQ(AshOsUrlHost(GURL("os://FlagS/foo/1")), "flags/foo");
  EXPECT_EQ(AshOsUrlHost(GURL("os://")), "");
  EXPECT_EQ(AshOsUrlHost(GURL("")), "");
  EXPECT_EQ(AshOsUrlHost(GURL("foo")), "");
  EXPECT_EQ(AshOsUrlHost(GURL("://")), "");
}

TEST(GurlOsHandlerUtilsTest, GetSystemUrlFromChromeUrl) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome", url::SCHEME_WITH_HOST);
  EXPECT_EQ(GetSystemUrlFromChromeUrl(GURL("chrome://flags/")),
            GURL("os://flags"));
  EXPECT_EQ(GetSystemUrlFromChromeUrl(GURL("chrome://flags/abc")),
            GURL("os://flags"));
  EXPECT_EQ(GetSystemUrlFromChromeUrl(GURL("chrome://flags?foo")),
            GURL("os://flags"));
  EXPECT_EQ(GetSystemUrlFromChromeUrl(GURL("chrome://foo")), GURL("os://foo"));
}

TEST(GurlOsHandlerUtilsTest, GetChromeUrlFromSystemUrl) {
  EXPECT_EQ(GetChromeUrlFromSystemUrl(GURL("os://flags/abc/def")),
            GURL("chrome://flags/abc"));
  EXPECT_EQ(GetChromeUrlFromSystemUrl(GURL("os://flags/abc")),
            GURL("chrome://flags/abc"));
  EXPECT_EQ(GetChromeUrlFromSystemUrl(GURL("os://flags?foo")),
            GURL("chrome://flags"));
  EXPECT_EQ(GetChromeUrlFromSystemUrl(GURL("os://foo")), GURL("chrome://foo"));
}

TEST(GurlOsHandlerUtilsTest, GetAshUrlFromLacrosUrl) {
  // To allow the "chrome" schemer, we need to add it to the registry.
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome", url::SCHEME_WITH_HOST);

  // os://settings need to be converted to chrome://os-settings
  EXPECT_EQ(GetTargetURLFromLacrosURL(GURL("os://settings/foo")),
            GURL("chrome://os-settings/foo"));
  // os-settings should not be changed
  EXPECT_EQ(GetTargetURLFromLacrosURL(GURL("os://os-settings")),
            GURL("os://os-settings"));
  // chrome://settings should also not be touched.
  EXPECT_EQ(GetTargetURLFromLacrosURL(GURL("chrome://settings/foo")),
            GURL("chrome://settings/foo"));
  // Needs to be sanitized.
  EXPECT_EQ(GetTargetURLFromLacrosURL(GURL("chrome://settings/foo#bar")),
            GURL("chrome://settings/foo"));
}

}  // namespace gurl_os_handler_utils

}  // namespace crosapi
