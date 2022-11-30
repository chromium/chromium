// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/omnibox_formatting.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "chrome/browser/vr/ui_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/render_text_test_api.h"
#include "ui/gfx/text_utils.h"

namespace vr {

namespace {

constexpr SkColor kDefaultColor = 0xFF000001;
constexpr SkColor kUrlColor = 0xFF000002;
constexpr SkColor kEmphasizedColor = 0xFF000003;
constexpr SkColor kDeemphasizedColor = 0xFF000004;

constexpr bool kNoOffset = false;
constexpr bool kHasOffset = true;

}  // namespace

TEST(OmniboxSuggestionFormatting, TextFormatting) {
  ACMatchClassifications classifications = {
      ACMatchClassification(0, ACMatchClassification::NONE),
      ACMatchClassification(1, ACMatchClassification::URL),
      ACMatchClassification(2, ACMatchClassification::MATCH),
  };
  size_t string_length = classifications.size();

  ColorScheme color_scheme;
  memset(&color_scheme, 0, sizeof(color_scheme));
  color_scheme.url_bar_text = kDefaultColor;
  color_scheme.hyperlink = kUrlColor;

  TextFormatting formatting =
      ConvertClassification(classifications, string_length, color_scheme);

  ASSERT_EQ(formatting.size(), 4u);

  EXPECT_EQ(formatting[0].type(), TextFormattingAttribute::COLOR);
  EXPECT_EQ(formatting[0].color(), kDefaultColor);
  EXPECT_EQ(formatting[0].range(), gfx::Range(0, string_length));

  EXPECT_EQ(formatting[1].type(), TextFormattingAttribute::DIRECTIONALITY);
  EXPECT_EQ(formatting[1].directionality(), gfx::DIRECTIONALITY_AS_URL);
  EXPECT_EQ(formatting[1].range(), gfx::Range(0, 0));

  EXPECT_EQ(formatting[2].type(), TextFormattingAttribute::COLOR);
  EXPECT_EQ(formatting[2].color(), kUrlColor);
  EXPECT_EQ(formatting[2].range(), gfx::Range(1, 2));

  EXPECT_EQ(formatting[3].type(), TextFormattingAttribute::WEIGHT);
  EXPECT_EQ(formatting[3].weight(), gfx::Font::Weight::BOLD);
  EXPECT_EQ(formatting[3].range(), gfx::Range(2, 3));
}

struct ElisionTestcase {
  std::string url_string;
  std::string field_size_string;
  bool has_offset;
  bool fade_left;
  bool fade_right;
};

class ElisionTest : public ::testing::TestWithParam<ElisionTestcase> {};

TEST_P(ElisionTest, ProperOffsetAndFading) {
  constexpr int kCharacterWidth = 10;
  constexpr int kMinPathWidth = 3 * kCharacterWidth;

  gfx::FontList font_list;
  auto field_width = GetParam().field_size_string.size() * kCharacterWidth;
  gfx::Rect field(field_width, font_list.GetHeight());

  // Format the test URL.
  GURL gurl(base::UTF8ToUTF16(GetParam().url_string));
  ASSERT_TRUE(gurl.is_valid());
  url::Parsed parsed;
  const std::u16string text = FormatUrlForVr(gurl, &parsed);

  std::unique_ptr<gfx::RenderText> render_text =
      gfx::RenderText::CreateRenderText();
  render_text->SetFontList(font_list);
  render_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  render_text->SetDirectionalityMode(gfx::DIRECTIONALITY_AS_URL);
  render_text->SetText(text);
  render_text->SetDisplayRect(field);
  render_text->SetCursorEnabled(false);

  // Use a fixed glyph width for testing to avoid having to measure text.
  gfx::test::RenderTextTestApi render_text_test_api(render_text.get());
  render_text_test_api.SetGlyphWidth(kCharacterWidth);

  ElisionParameters result =
      GetElisionParameters(gurl, parsed, render_text.get(), kMinPathWidth);

  EXPECT_EQ(result.offset != 0, GetParam().has_offset);
  EXPECT_EQ(result.fade_left, GetParam().fade_left);
  EXPECT_EQ(result.fade_right, GetParam().fade_right);
}

const std::vector<ElisionTestcase> elision_test_cases = {
    // URL exactly matches the field width.
    {"http://abc.com", "abc.com", kNoOffset, false, false},
    {"http://abc.com/aaa", "abc.com/aaa", kNoOffset, false, false},
    // URL is narrower than the field.
    {"http://abc.com/a", "abc.com/aaa", kNoOffset, false, false},
    // A really long path should not interfere with a short domain.
    {"http://abc.com/aaaaaaaaaaaaaaaaaaaaaaaaa", "abc.com/aaa", kNoOffset,
     false, true},
    // A long domain should be offset and fade on the left.
    {"http://aaaaaaaaaaaaaaaaaaaaaaaa.abc.com", "abc.com/aaa", kHasOffset, true,
     false},
    // A long domain with a tiny path should preserve the path.
    {"http://aaaaaaaaaaaaaaaaaaaaaaaa.abc.com/a", "abc.com/aaa", kHasOffset,
     true, false},
    // A long domain and path should see fading at both ends.
    {"http://aaaaaaaaaaaaaaaaaaaaaaaa.abc.com/aaaaaaaaaaaaaaaaaaaaaaaaaaaa",
     "abc.com/aaa", kHasOffset, true, true},
    // A file URL with no hostname should fade to the right.
    {"file:///path/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "file://path/aaa",
     kNoOffset, false, true},
    // A file URL with a hostname should treat the hostname like https.
    {"file://very-long-file-hostname/aaaaaaaaaaaaaa", "file://hostname/aaa",
     kHasOffset, true, true},
    {"data:text/plain;charset=UTF-8;aaaaaaaaaaaaaaaaaaa", "data:text/plain",
     kNoOffset, false, true},
};

INSTANTIATE_TEST_SUITE_P(ElisionTestCases,
                         ElisionTest,
                         ::testing::ValuesIn(elision_test_cases));

TextFormatting CreateTextUrlFormatting(const std::string& url_string,
                                       const std::string& expected_string) {
  GURL url(base::UTF8ToUTF16(url_string));
  url::Parsed parsed;
  const std::u16string formatted_url = FormatUrlForVr(url, &parsed);
  EXPECT_EQ(formatted_url, base::UTF8ToUTF16(expected_string));
  return CreateUrlFormatting(formatted_url, parsed, kEmphasizedColor,
                             kDeemphasizedColor);
}

TEST(UrlFormatting, HttpUrlHasHostEmphasized) {
  TextFormatting formatting =
      CreateTextUrlFormatting("https://host.com/page", "host.com/page");
  ASSERT_EQ(formatting.size(), 2u);
  EXPECT_EQ(formatting[0], TextFormattingAttribute(kDeemphasizedColor,
                                                   gfx::Range::InvalidRange()));
  EXPECT_EQ(formatting[1],
            TextFormattingAttribute(kEmphasizedColor, gfx::Range(0, 8)));
}

TEST(UrlFormatting, FileUrlIsAllEmphasized) {
  TextFormatting formatting =
      CreateTextUrlFormatting("file:///abc/def", "file:///abc/def");
  ASSERT_EQ(formatting.size(), 1u);
  EXPECT_EQ(formatting[0], TextFormattingAttribute(kEmphasizedColor,
                                                   gfx::Range::InvalidRange()));
}

TEST(UrlFormatting, DataUrlHasSchemeEmphasized) {
  TextFormatting formatting = CreateTextUrlFormatting(
      "data:text/html,lots of data", "data:text/html,lots of data");
  ASSERT_EQ(formatting.size(), 2u);
  EXPECT_EQ(formatting[0], TextFormattingAttribute(kDeemphasizedColor,
                                                   gfx::Range::InvalidRange()));
  EXPECT_EQ(formatting[1],
            TextFormattingAttribute(kEmphasizedColor, gfx::Range(0, 4)));
}

}  // namespace vr
