// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/favicon_url_parser.h"

#include <memory>

#include "components/favicon_base/favicon_types.h"
#include "testing/gtest/include/gtest/gtest.h"

class FaviconUrlParserTest : public testing::Test {
 public:
  FaviconUrlParserTest() = default;

  FaviconUrlParserTest(const FaviconUrlParserTest&) = delete;
  FaviconUrlParserTest& operator=(const FaviconUrlParserTest&) = delete;

  ~FaviconUrlParserTest() override = default;
};

// Test parsing path with no extra parameters.
TEST_F(FaviconUrlParserTest, LegacyParsingNoExtraParams) {
  const std::string url("https://www.google.ca/imghp?hl=en&tab=wi");
  chrome::ParsedFaviconPath parsed;

  const std::string path1 = url;
  EXPECT_TRUE(chrome::ParseFaviconPath(
      path1, chrome::FaviconUrlFormat::kFaviconLegacy, &parsed));
  EXPECT_EQ(parsed.icon_url, "");
  EXPECT_EQ(url, parsed.page_url);
  EXPECT_EQ(16, parsed.size_in_dip);
  EXPECT_EQ(1.0f, parsed.device_scale_factor);
}

// Test parsing path with a 'size' parameter.
TEST_F(FaviconUrlParserTest, LegacyParsingSizeParam) {
  const std::string url("https://www.google.ca/imghp?hl=en&tab=wi");
  chrome::ParsedFaviconPath parsed;

  // Test that we can still parse the legacy 'size' parameter format.
  const std::string path2 = "size/32/" + url;
  EXPECT_TRUE(chrome::ParseFaviconPath(
      path2, chrome::FaviconUrlFormat::kFaviconLegacy, &parsed));
  EXPECT_EQ(parsed.icon_url, "");
  EXPECT_EQ(url, parsed.page_url);
  EXPECT_EQ(32, parsed.size_in_dip);
  EXPECT_EQ(1.0f, parsed.device_scale_factor);

  // Test parsing current 'size' parameter format.
  const std::string path3 = "size/32@1.4x/" + url;
  EXPECT_TRUE(chrome::ParseFaviconPath(
      path3, chrome::FaviconUrlFormat::kFaviconLegacy, &parsed));
  EXPECT_EQ(parsed.icon_url, "");
  EXPECT_EQ(url, parsed.page_url);
  EXPECT_EQ(32, parsed.size_in_dip);
  EXPECT_EQ(1.4f, parsed.device_scale_factor);

  // Test that we pick the ui::ResourceScaleFactor which is closest to the
  // passed in scale factor.
  const std::string path4 = "size/16@1.41x/" + url;
  EXPECT_TRUE(chrome::ParseFaviconPath(
      path4, chrome::FaviconUrlFormat::kFaviconLegacy, &parsed));
  EXPECT_EQ(parsed.icon_url, "");
  EXPECT_EQ(url, parsed.page_url);
  EXPECT_EQ(16, parsed.size_in_dip);
  EXPECT_EQ(1.41f, parsed.device_scale_factor);

  // Invalid cases.
  const std::string path5 = "size/" + url;
  EXPECT_FALSE(chrome::ParseFaviconPath(
      path5, chrome::FaviconUrlFormat::kFaviconLegacy, &parsed));
  const std::string path6 = "size/@1x/" + url;
  EXPECT_FALSE(chrome::ParseFaviconPath(
      path6, chrome::FaviconUrlFormat::kFaviconLegacy, &parsed));
  const std::string path7 = "size/abc@1x/" + url;
  EXPECT_FALSE(chrome::ParseFaviconPath(
      path7, chrome::FaviconUrlFormat::kFaviconLegacy, &parsed));

  // Part of url looks like 'size' parameter.
  const std::string path8 = "http://www.google.com/size/32@1.4x";
  EXPECT_TRUE(chrome::ParseFaviconPath(
      path8, chrome::FaviconUrlFormat::kFaviconLegacy, &parsed));
  EXPECT_EQ(parsed.icon_url, "");
  EXPECT_EQ(path8, parsed.page_url);
  EXPECT_EQ(16, parsed.size_in_dip);
  EXPECT_EQ(1.0f, parsed.device_scale_factor);
}

// Test parsing path with 'iconurl' parameter.
TEST_F(FaviconUrlParserTest, LegacyParsingIconUrlParam) {
  const std::string url("https://www.google.ca/imghp?hl=en&tab=wi");
  chrome::ParsedFaviconPath parsed;

  const std::string path10 = "iconurl/http://www.google.com/favicon.ico";
  EXPECT_TRUE(chrome::ParseFaviconPath(
      path10, chrome::FaviconUrlFormat::kFaviconLegacy, &parsed));
  EXPECT_EQ("http://www.google.com/favicon.ico", parsed.icon_url);
  EXPECT_EQ("", parsed.page_url);
  EXPECT_EQ(16, parsed.size_in_dip);
  EXPECT_EQ(1.0f, parsed.device_scale_factor);
}

// Test parsing paths with both a 'size' parameter and a 'url modifier'
// parameter.
TEST_F(FaviconUrlParserTest, LegacyParsingSizeParamAndUrlModifier) {
  const std::string url("https://www.google.ca/imghp?hl=en&tab=wi");
  chrome::ParsedFaviconPath parsed;

  const std::string path14 =
      "size/32/iconurl/http://www.google.com/favicon.ico";
  EXPECT_TRUE(chrome::ParseFaviconPath(
      path14, chrome::FaviconUrlFormat::kFaviconLegacy, &parsed));
  EXPECT_EQ("http://www.google.com/favicon.ico", parsed.icon_url);
  EXPECT_EQ("", parsed.page_url);
  EXPECT_EQ(32, parsed.size_in_dip);
}

TEST_F(FaviconUrlParserTest, Favicon2ParsingSizeParam) {
  chrome::ParsedFaviconPath parsed;

  EXPECT_TRUE(chrome::ParseFaviconPath("?size=32&page_url=https%3A%2F%2Fg.com",
                                       chrome::FaviconUrlFormat::kFavicon2,
                                       &parsed));
  EXPECT_EQ(32, parsed.size_in_dip);

  EXPECT_FALSE(
      chrome::ParseFaviconPath("?size=abc&page_url=https%3A%2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
}

TEST_F(FaviconUrlParserTest, Favicon2ParsingScaleFactorParam) {
  chrome::ParsedFaviconPath parsed;

  EXPECT_TRUE(chrome::ParseFaviconPath(
      "?scale_factor=2.1x&page_url=https%3A%2F%2Fg.com",
      chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_EQ(2.1f, parsed.device_scale_factor);

  EXPECT_FALSE(
      chrome::ParseFaviconPath("?scale_factor=-1&page_url=https%3A%2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
}

TEST_F(FaviconUrlParserTest, Favicon2ParsingUrlParams) {
  chrome::ParsedFaviconPath parsed;

  EXPECT_TRUE(
      chrome::ParseFaviconPath("?icon_url=https%3A%2F%2Fg.com%2Ffavicon.ico",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_EQ(parsed.icon_url, "https://g.com/favicon.ico");
  EXPECT_EQ(parsed.page_url, "");

  EXPECT_TRUE(chrome::ParseFaviconPath("?page_url=https%3A%2F%2Fg.com",
                                       chrome::FaviconUrlFormat::kFavicon2,
                                       &parsed));
  EXPECT_EQ(parsed.icon_url, "");
  EXPECT_EQ(parsed.page_url, "https://g.com");
}

TEST_F(FaviconUrlParserTest, Favicon2ParsingAllowFallbackParam) {
  chrome::ParsedFaviconPath parsed;

  EXPECT_FALSE(chrome::ParseFaviconPath(
      "?allow_google_server_fallback=invalid&page_url=https%"
      "3A%2F%2Fg.com",
      chrome::FaviconUrlFormat::kFavicon2, &parsed));

  EXPECT_TRUE(chrome::ParseFaviconPath(
      "?allow_google_server_fallback=0&page_url=https%3A%"
      "2F%2Fg.com",
      chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_FALSE(parsed.allow_favicon_server_fallback);

  EXPECT_TRUE(chrome::ParseFaviconPath(
      "?allow_google_server_fallback=1&page_url=https%3A%"
      "2F%2Fg.com",
      chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_TRUE(parsed.allow_favicon_server_fallback);
}

TEST_F(FaviconUrlParserTest, Favicon2ParsingShowFallbackMonogram) {
  chrome::ParsedFaviconPath parsed;

  parsed.show_fallback_monogram = true;
  EXPECT_TRUE(chrome::ParseFaviconPath("?page_url=https%3A%2F%2Fg.com",
                                       chrome::FaviconUrlFormat::kFavicon2,
                                       &parsed));
  EXPECT_FALSE(parsed.show_fallback_monogram);

  parsed.show_fallback_monogram = false;
  EXPECT_TRUE(
      chrome::ParseFaviconPath("?show_fallback_monogram&page_url=https%3A%"
                               "2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_TRUE(parsed.show_fallback_monogram);
}
