// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_helpers.h"

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

  EXPECT_EQ(
      "ghmpeckcpimfdekfodogbnnpmkppngmo",
      GenerateAppId(/*manifest_id_path=*/std::nullopt, GURL(subapp_starturl)));

  EXPECT_EQ("fekpfidpgacknlbhejgeblllclomiekk",
            GenerateAppId("manifest.webmanifest", GURL(subapp_starturl)));
}

TEST(WebAppHelpers, GenerateManifestIdFromStartUrlOnly) {
  EXPECT_EQ(
      GURL("https://example.com/").spec(),
      GenerateManifestIdFromStartUrlOnly(GURL("https://example.com/")).spec());
  EXPECT_EQ(
      GURL("https://example.com").spec(),
      GenerateManifestIdFromStartUrlOnly(GURL("https://example.com")).spec());
  EXPECT_EQ(
      GURL("https://example.com/start?a=b").spec(),
      GenerateManifestIdFromStartUrlOnly(GURL("https://example.com/start?a=b"))
          .spec());
  EXPECT_EQ(GURL("https://example.com/start").spec(),
            GenerateManifestIdFromStartUrlOnly(
                GURL("https://example.com/start#fragment"))
                .spec());
}

TEST(WebAppHelpers, ManifestIdEncoding) {
  GURL start_url("https://example.com/abc");
  // ASCII character. URL parser no longer unescapes percent encoded ASCII
  // characters. See https://crbug.com/40198802.
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

  EXPECT_EQ(url.spec(), GenerateManifestIdFromStartUrlOnly(url).spec());
  EXPECT_EQ(url.spec(),
            GenerateManifestIdFromStartUrlOnly(url_with_fragment).spec());
  EXPECT_EQ(url_with_query.spec(),
            GenerateManifestIdFromStartUrlOnly(url_with_query).spec());
  EXPECT_EQ(
      url_with_query.spec(),
      GenerateManifestIdFromStartUrlOnly(url_with_query_and_fragment).spec());

  EXPECT_EQ(url.spec(), GenerateManifestId("test", start_url_long).spec());
  EXPECT_EQ(url.spec(), GenerateManifestId("test#id", start_url_long).spec());
  EXPECT_EQ(url_with_query.spec(),
            GenerateManifestId("test?id", start_url_long).spec());
  EXPECT_EQ(url_with_query.spec(),
            GenerateManifestId("test?id#fragment", start_url_long).spec());
}

}  // namespace web_app
