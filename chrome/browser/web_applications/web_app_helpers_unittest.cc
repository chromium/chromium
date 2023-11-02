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
            GenerateAppId(/*manifest_id=*/absl::nullopt,
                          GURL("https://www.chromestatus.com/features")));

  // The io2016 example is also walked through at
  // https://play.golang.org/p/VrIq_QKFjiV
  EXPECT_EQ("mjgafbdfajpigcjmkgmeokfbodbcfijl",
            GenerateAppId(/*manifest_id=*/absl::nullopt,
                          GURL("https://events.google.com/io2016/"
                               "?utm_source=web_app_manifest")));
}

TEST(WebAppHelpers, GenerateRecommendedId) {
  EXPECT_EQ("", GenerateRecommendedId(GURL()));
  EXPECT_EQ("/", GenerateRecommendedId(GURL("https://example.com/")));
  EXPECT_EQ("/", GenerateRecommendedId(GURL("https://example.com")));
  EXPECT_EQ("/start?a=b",
            GenerateRecommendedId(GURL("https://example.com/start?a=b")));
}

TEST(WebAppHelpers, IsValidWebAppUrl) {
  // TODO(crbug.com/1253234): Remove chrome-extension scheme.
  EXPECT_TRUE(IsValidWebAppUrl(
      GURL("chrome-extension://oafaagfgbdpldilgjjfjocjglfbolmac")));

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
  // ASCII character.
  EXPECT_EQ(GenerateAppId("j", start_url), GenerateAppId("%6a", start_url));
  EXPECT_EQ(GenerateAppId("%6Ax", start_url), GenerateAppId("%6ax", start_url));

  // Special characters.
  EXPECT_EQ(GenerateAppId("a😀b", start_url),
            GenerateAppId("a%F0%9F%98%80b", start_url));
  EXPECT_EQ(GenerateAppId("a b", start_url), GenerateAppId("a%20b", start_url));

  // "/"" is excluded from encoding according to url spec.
  EXPECT_NE(GenerateAppId("a/b", start_url), GenerateAppId("a%2Fb", start_url));
}
}  // namespace web_app
