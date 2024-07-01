// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_helpers.h"

#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

TEST(WebAppHelpers, GenerateApplicationNameFromURL) {
  EXPECT_EQ("_", GenerateApplicationNameFromURL(GURL()));

  EXPECT_EQ("example.com_/",
            GenerateApplicationNameFromURL(GURL("http://example.com")));

  EXPECT_EQ("example.com_/path",
            GenerateApplicationNameFromURL(GURL("https://example.com/path")));
}

TEST(WebAppHelpers, GenerateAppId) {
  EXPECT_EQ("fedbieoalmbobgfjapopkghdmhgncnaa",
            GenerateAppId(/*manifest_id=*/std::nullopt,
                          GURL("https://www.chromestatus.com/features")));

  // The io2016 example is also walked through at
  // https://play.golang.org/p/VrIq_QKFjiV
  EXPECT_EQ("mjgafbdfajpigcjmkgmeokfbodbcfijl",
            GenerateAppId(/*manifest_id=*/std::nullopt,
                          GURL("https://events.google.com/io2016/"
                               "?utm_source=web_app_manifest")));
}

TEST(WebAppHelpers, GenerateAppIdForSubApps) {
  const std::string subapp_starturl = "https://example.com/subapp";
  const webapps::ManifestId parent_manifest_id = GURL("https://example.com");

  EXPECT_EQ("emdpgjhffapdncpmnindbhiapcohmjga",
            GenerateAppId(/*manifest_id_path=*/std::nullopt,
                          GURL(subapp_starturl), parent_manifest_id));

  EXPECT_EQ("jaadilplijgkeakjaoplplaeceoommee",
            GenerateAppId("manifest.webmanifest", GURL(subapp_starturl),
                          parent_manifest_id));
}

TEST(WebAppHelpers, GenerateManifestIdFromStartUrlOnly) {
  EXPECT_EQ(GURL("https://example.com/"),
            GenerateManifestIdFromStartUrlOnly(GURL("https://example.com/")));
  EXPECT_EQ(GURL("https://example.com"),
            GenerateManifestIdFromStartUrlOnly(GURL("https://example.com")));
  EXPECT_EQ(GURL("https://example.com/start?a=b"),
            GenerateManifestIdFromStartUrlOnly(
                GURL("https://example.com/start?a=b")));
  EXPECT_EQ(GURL("https://example.com/start"),
            GenerateManifestIdFromStartUrlOnly(
                GURL("https://example.com/start#fragment")));
}

TEST(WebAppHelpers, IsValidWebAppUrl) {
  // TODO(crbug.com/40793595): Remove chrome-extension scheme.
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(IsValidWebAppUrl(
      GURL("chrome-extension://oafaagfgbdpldilgjjfjocjglfbolmac")));
#else
  // With ShortcutsNotApps enabled, chrome-extension:// URLs can only be
  // shortcuts rather than web apps.
  EXPECT_NE(IsValidWebAppUrl(
                GURL("chrome-extension://oafaagfgbdpldilgjjfjocjglfbolmac")),
            base::FeatureList::IsEnabled(features::kShortcutsNotApps));
#endif  // BUILDFLAG(IS_CHROMEOS)

  EXPECT_TRUE(IsValidWebAppUrl(GURL("https://chromium.org")));
  EXPECT_TRUE(IsValidWebAppUrl(GURL("https://www.chromium.org")));
  EXPECT_TRUE(
      IsValidWebAppUrl(GURL("https://www.chromium.org/path/to/page.html")));
  EXPECT_TRUE(IsValidWebAppUrl(GURL("http://chromium.org")));
  EXPECT_TRUE(IsValidWebAppUrl(GURL("http://www.chromium.org")));
  EXPECT_TRUE(
      IsValidWebAppUrl(GURL("http://www.chromium.org/path/to/page.html")));
  EXPECT_TRUE(IsValidWebAppUrl(GURL("https://examle.com/foo?bar")));
  EXPECT_TRUE(IsValidWebAppUrl(GURL("https://examle.com/foo#bar")));

  EXPECT_FALSE(IsValidWebAppUrl(GURL()));
  EXPECT_FALSE(IsValidWebAppUrl(GURL("ftp://www.chromium.org")));
  EXPECT_FALSE(IsValidWebAppUrl(GURL("chrome://flags")));
  EXPECT_FALSE(IsValidWebAppUrl(GURL("about:blank")));
  EXPECT_FALSE(
      IsValidWebAppUrl(GURL("file://mhjfbmdgcfjbbpaeojofohoefgiehjai")));
  EXPECT_FALSE(IsValidWebAppUrl(GURL("chrome://extensions")));
  EXPECT_FALSE(
      IsValidWebAppUrl(GURL("filesystem:http://example.com/path/file.html")));
  EXPECT_TRUE(IsValidWebAppUrl(GURL("chrome://password-manager")));
}

TEST(WebAppHelpers, ManifestIdEncoding) {
  GURL start_url("https://example.com/abc");
  // ASCII character. URL parser no longer unescapes percent encoded ASCII
  // characters. See https://crbug.com/1252531.
  EXPECT_EQ(GenerateAppId("j", start_url), GenerateAppId("j", start_url));
  EXPECT_EQ(GenerateAppId("%6Ax", start_url), GenerateAppId("%6Ax", start_url));

  // Special characters.
  EXPECT_EQ(GenerateAppId("a😀b", start_url),
            GenerateAppId("a%F0%9F%98%80b", start_url));
  EXPECT_EQ(GenerateAppId("a b", start_url), GenerateAppId("a%20b", start_url));

  // "/"" is excluded from encoding according to url spec.
  EXPECT_NE(GenerateAppId("a/b", start_url), GenerateAppId("a%2Fb", start_url));
}

TEST(WebAppHelpers, ManifestIdWithQueriesAndFragments) {
  GURL start_url_long = GURL("https://example.com/start_url/long/path.html");
  GURL url = GURL("https://example.com/test");
  GURL url_with_query = GURL("https://example.com/test?id");
  GURL url_with_fragment = GURL("https://example.com/test#id");
  GURL url_with_query_and_fragment =
      GURL("https://example.com/test?id#fragment");

  EXPECT_EQ(url, GenerateManifestIdFromStartUrlOnly(url));
  EXPECT_EQ(url, GenerateManifestIdFromStartUrlOnly(url_with_fragment));
  EXPECT_EQ(url_with_query, GenerateManifestIdFromStartUrlOnly(url_with_query));
  EXPECT_EQ(url_with_query,
            GenerateManifestIdFromStartUrlOnly(url_with_query_and_fragment));

  EXPECT_EQ(url, GenerateManifestId("test", start_url_long));
  EXPECT_EQ(url, GenerateManifestId("test#id", start_url_long));
  EXPECT_EQ(url_with_query, GenerateManifestId("test?id", start_url_long));
  EXPECT_EQ(url_with_query,
            GenerateManifestId("test?id#fragment", start_url_long));
}

}  // namespace web_app
