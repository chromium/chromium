// Copyright 2013 The Chromium Authors
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

  // Test that negative sizes lead to parsing errors.
  const std::string path9 = "size/-32/" + url;
  EXPECT_FALSE(chrome::ParseFaviconPath(
      path9, chrome::FaviconUrlFormat::kFaviconLegacy, &parsed));

  // Test that size zero leads to parsing errors.
  const std::string path10 = "size/0/" + url;
  EXPECT_FALSE(chrome::ParseFaviconPath(
      path10, chrome::FaviconUrlFormat::kFaviconLegacy, &parsed));

  // Test that negative scale factors lead to parsing errors.
  const std::string path11 = "size/32@-1.4x/" + url;
  EXPECT_FALSE(chrome::ParseFaviconPath(
      path11, chrome::FaviconUrlFormat::kFaviconLegacy, &parsed));

  // Test that scale factor zero leads to parsing errors.
  const std::string path12 = "size/32@0x/" + url;
  EXPECT_FALSE(chrome::ParseFaviconPath(
      path12, chrome::FaviconUrlFormat::kFaviconLegacy, &parsed));
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

  EXPECT_TRUE(chrome::ParseFaviconPath("?size=32&pageUrl=https%3A%2F%2Fg.com",
                                       chrome::FaviconUrlFormat::kFavicon2,
                                       &parsed));
  EXPECT_EQ(32, parsed.size_in_dip);

  EXPECT_FALSE(chrome::ParseFaviconPath("?size=abc&pageUrl=https%3A%2F%2Fg.com",
                                        chrome::FaviconUrlFormat::kFavicon2,
                                        &parsed));

  EXPECT_FALSE(chrome::ParseFaviconPath("?size=-32&pageUrl=https%3A%2F%2Fg.com",
                                        chrome::FaviconUrlFormat::kFavicon2,
                                        &parsed));
}

TEST_F(FaviconUrlParserTest, Favicon2ParsingScaleFactorParam) {
  chrome::ParsedFaviconPath parsed;

  EXPECT_TRUE(
      chrome::ParseFaviconPath("?scaleFactor=2.1x&pageUrl=https%3A%2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_EQ(2.1f, parsed.device_scale_factor);

  // Missing 'x' in scale factor (parsing error).
  EXPECT_FALSE(
      chrome::ParseFaviconPath("?scaleFactor=2.1&pageUrl=https%3A%2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));

  // Negative scale factor (parsing error).
  EXPECT_FALSE(
      chrome::ParseFaviconPath("?scaleFactor=-2.1x&pageUrl=https%3A%2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));

  // Scale factor zero (parsing error).
  EXPECT_FALSE(
      chrome::ParseFaviconPath("?scaleFactor=0x&pageUrl=https%3A%2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
}

TEST_F(FaviconUrlParserTest, Favicon2ParsingUrlParams) {
  chrome::ParsedFaviconPath parsed;

  EXPECT_TRUE(
      chrome::ParseFaviconPath("?iconUrl=https%3A%2F%2Fg.com%2Ffavicon.ico",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_EQ(parsed.icon_url, "https://g.com/favicon.ico");
  EXPECT_EQ(parsed.page_url, "");

  EXPECT_TRUE(chrome::ParseFaviconPath("?pageUrl=https%3A%2F%2Fg.com",
                                       chrome::FaviconUrlFormat::kFavicon2,
                                       &parsed));
  EXPECT_EQ(parsed.icon_url, "");
  EXPECT_EQ(parsed.page_url, "https://g.com");
}

TEST_F(FaviconUrlParserTest, Favicon2ParsingAllowFallbackParam) {
  chrome::ParsedFaviconPath parsed;

  EXPECT_FALSE(chrome::ParseFaviconPath(
      "?allowGoogleServerFallback=invalid&pageUrl=https%"
      "3A%2F%2Fg.com",
      chrome::FaviconUrlFormat::kFavicon2, &parsed));

  EXPECT_TRUE(
      chrome::ParseFaviconPath("?allowGoogleServerFallback=0&pageUrl=https%3A%"
                               "2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_FALSE(parsed.allow_favicon_server_fallback);

  EXPECT_TRUE(
      chrome::ParseFaviconPath("?allowGoogleServerFallback=1&pageUrl=https%3A%"
                               "2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_TRUE(parsed.allow_favicon_server_fallback);
}

TEST_F(FaviconUrlParserTest, Favicon2ParsingShowFallbackMonogram) {
  chrome::ParsedFaviconPath parsed;

  parsed.show_fallback_monogram = true;
  EXPECT_TRUE(chrome::ParseFaviconPath("?pageUrl=https%3A%2F%2Fg.com",
                                       chrome::FaviconUrlFormat::kFavicon2,
                                       &parsed));
  EXPECT_FALSE(parsed.show_fallback_monogram);

  parsed.show_fallback_monogram = false;
  EXPECT_TRUE(
      chrome::ParseFaviconPath("?showFallbackMonogram&pageUrl=https%3A%"
                               "2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_TRUE(parsed.show_fallback_monogram);
}

TEST_F(FaviconUrlParserTest, Favicon2ParsingForceLightMode) {
  chrome::ParsedFaviconPath parsed;

  parsed.force_light_mode = true;
  EXPECT_TRUE(chrome::ParseFaviconPath("?pageUrl=https%3A%2F%2Fg.com",
                                       chrome::FaviconUrlFormat::kFavicon2,
                                       &parsed));
  EXPECT_FALSE(parsed.force_light_mode);

  parsed.force_light_mode = false;
  EXPECT_TRUE(
      chrome::ParseFaviconPath("?forceLightMode&pageUrl=https%3A%"
                               "2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_TRUE(parsed.force_light_mode);
}
