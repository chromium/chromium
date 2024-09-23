// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"

#include <ostream>
#include <string>

#include "build/build_config.h"
#include "chrome/browser/ui/views/tabs/filename_elider.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/render_text.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

class TabHoverCardBubbleViewFilenameEliderTest {
 protected:
  static constexpr float kGlyphWidth = 10.0;
  static constexpr float kGlyphHeight = 10.0;

  static std::unique_ptr<gfx::RenderText> CreateRenderText(
      const std::u16string& text) {
    auto render_text = gfx::RenderText::CreateRenderText();
    render_text->set_glyph_width_for_test(kGlyphWidth);
    render_text->set_glyph_height_for_test(kGlyphHeight);
    if (!text.empty())
      render_text->SetText(text);
    return render_text;
  }

  static gfx::Rect GetTextRect(int num_chars_per_line) {
    // Bump up the width to almost large enough to hold an extra character in
    // order to avoid some quirkiness with RenderText even when there's a fixed
    // glyph width.
    return gfx::Rect(kGlyphWidth * (num_chars_per_line + 0.9f),
                     kGlyphHeight * 2);
  }

  static FilenameElider::LineLengths GetLineLengths(const std::u16string& text,
                                                    int num_chars_per_line) {
    FilenameElider elider(CreateRenderText(text));
    return elider.GetLineLengths(GetTextRect(num_chars_per_line));
  }

  static std::u16string ElideImpl(const std::u16string& text,
                                  size_t max_first_line_length,
                                  size_t max_second_line_length) {
    FilenameElider elider(CreateRenderText(text));
    return elider.ElideImpl(FilenameElider::LineLengths{
        max_first_line_length, max_second_line_length});
  }

 private:
  // Required for loading fallback fonts, as fallback font loading needs to
  // happen on the UI thread.
  content::BrowserTaskEnvironment task_environment_;
};

#define EM_SPACE u"\u2003"
#define MULT_SYMBOL u"\u00D7"
#define COMBINING_CIRCUMFLEX u"\u0302"

#define MEDICAL_SYMBOL_EMOJI u"\u2695\uFE0F"
#define ZERO_WIDTH_JOINER u"\u200D"
#define MAN_EMOJI u"\U0001F468"
#define MEDIUM_SKIN_TONE_MODIFIER u"\U0001F3FD"
#define MALE_HEALTH_WORKER_MEDIUM_SKIN_TONE \
  MAN_EMOJI MEDIUM_SKIN_TONE_MODIFIER ZERO_WIDTH_JOINER MEDICAL_SYMBOL_EMOJI

#define BIDIFY(a, b) a u"\n\u2068" b u"\u2069"
#define ELLIPSIZE(a, b) BIDIFY(a, u"\u2026" b)

struct ElideImplTestParams {
  const char16_t* const text;
  const size_t max_first_line_length;
  const size_t max_second_line_length;
  const char16_t* const expected;
  const char* const comment;
};

void PrintTo(const ElideImplTestParams& params, ::std::ostream* os) {
  *os << params.comment << " (\"" << params.text << "\", "
      << params.max_first_line_length << ", " << params.max_second_line_length
      << ")";
}

const ElideImplTestParams kElideImplTestParams[]{
    {u"", 0, 0, u"", "Zero-length string yields empty result."},
    {u"abcd", 0, 0, u"", "Zero-length lines yield empty results."},
    {u"abcd", 4, 0, u"abcd",
     "First line is all text yields full text, even if second is empty."},
    {u"abcd", 0, 4, u"abcd",
     "Second line is all text yields full text, even if first is empty."},
    {u"abcd", 4, 1, u"abcd", "First line is all text yields full text."},
    {u"abcd", 1, 4, u"abcd", "Second line is all text yields full text."},
    {u"abcd", 1, 1, ELLIPSIZE(u"a", u"d"),
     "Gap between first and second lines."},
    {u"abcd", 2, 2, u"ab\ncd", "No gap between first and second lines."},
    {u"abcd", 3, 3, u"abc\nd", "Overlap between first and second lines."},
    {u"abc.def", 3, 3, ELLIPSIZE(u"abc", u"def"), "Extension dot is cut out."},
    {u"abc.def", 4, 3, u"abc.\ndef",
     "Extension whole string dot is not moved."},
    {u"abc.def", 4, 4, BIDIFY(u"abc", u".def"),
     "Extension overlap dot moved to second line."},
    {u"abc.def", 6, 6, BIDIFY(u"abc", u".def"),
     "Extension overlap dot moved to second line (2)."},
    {u"abc def", 3, 4, u"abc\ndef", "Whitespace after break is elided (1)."},
    {u"abc\t" EM_SPACE u"def", 3, 4, ELLIPSIZE(u"abc", u"def"),
     "Whitespace after break is elided (2)."},
    {u"abc\t" EM_SPACE u"def", 3, 5, u"abc\ndef",
     "Whitespace after break is elided (3)."},
    {u"abc def", 2, 4, ELLIPSIZE(u"ab", u"def"),
     "Whitespace after ellipsis is elided (1)."},
    {u"abc\t" EM_SPACE u"def", 2, 4, ELLIPSIZE(u"ab", u"def"),
     "Whitespace after ellipsis is elided (2)."},
    {u"abc\t" EM_SPACE u"def", 2, 5, ELLIPSIZE(u"ab", u"def"),
     "Whitespace after ellipsis is elided (3)."},
    {u"abco" COMBINING_CIRCUMFLEX u"def", 3, 5,
     u"abc\no" COMBINING_CIRCUMFLEX u"def",
     "Cut before combining characters does not elide characters."},
    {u"abco" COMBINING_CIRCUMFLEX u"def", 5, 3,
     u"abco" COMBINING_CIRCUMFLEX u"\ndef",
     "Cut after combining characters does not elide characters."},
    {u"abc" MAN_EMOJI u"def", 3, 5, u"abc\n" MAN_EMOJI u"def",
     "Cut before four-byte emoji does not elide emoji."},
    {u"abc" MAN_EMOJI u"def", 5, 3, u"abc" MAN_EMOJI u"\ndef",
     "Cut after four-byte emoji does not elide emoji."},
    {u"abc" MAN_EMOJI MAN_EMOJI MAN_EMOJI u"def", 3, 3,
     ELLIPSIZE(u"abc", u"def"), "Cut around multiple emoji."},
    {u"abc" MAN_EMOJI MAN_EMOJI MAN_EMOJI u"def", 5, 3,
     ELLIPSIZE(u"abc" MAN_EMOJI, u"def"),
     "Cut the end of a sequence of emoji."},
    {u"abc" MAN_EMOJI MAN_EMOJI MAN_EMOJI u"def", 5, 5,
     ELLIPSIZE(u"abc" MAN_EMOJI, MAN_EMOJI u"def"),
     "Cut the middle of a sequence of emoji."},
    {u"abc" MEDICAL_SYMBOL_EMOJI u"def", 3, 5,
     u"abc\n" MEDICAL_SYMBOL_EMOJI u"def",
     "Cut before two-character emoji does not elide emoji."},
    {u"abc" MEDICAL_SYMBOL_EMOJI u"def", 5, 3,
     u"abc" MEDICAL_SYMBOL_EMOJI u"\ndef",
     "Cut after two-character emoji does not elide emoji."},
    {u"abc" MAN_EMOJI MEDIUM_SKIN_TONE_MODIFIER u"def", 3, 7,
     u"abc\n" MAN_EMOJI MEDIUM_SKIN_TONE_MODIFIER u"def",
     "Cut before modified emoji does not elide emoji."},
    {u"abc" MAN_EMOJI MEDIUM_SKIN_TONE_MODIFIER u"def", 7, 3,
     u"abc" MAN_EMOJI MEDIUM_SKIN_TONE_MODIFIER u"\ndef",
     "Cut after modified emoji does not elide emoji."},
    {u"abc" MALE_HEALTH_WORKER_MEDIUM_SKIN_TONE u"def", 3, 10,
     u"abc\n" MALE_HEALTH_WORKER_MEDIUM_SKIN_TONE u"def",
     "Cut before compound emoji does not elide emoji."},
    {u"abc" MALE_HEALTH_WORKER_MEDIUM_SKIN_TONE u"def", 10, 3,
     u"abc" MALE_HEALTH_WORKER_MEDIUM_SKIN_TONE u"\ndef",
     "Cut after compound emoji does not elide emoji."},
    {u"abco" COMBINING_CIRCUMFLEX u" def", 3, 4, ELLIPSIZE(u"abc", u"def"),
     "Eliminates whitespace after elided combining character."},
    {u"abc.png (123" MULT_SYMBOL u"456)", 15, 15,
     BIDIFY(u"abc.png ", u"(123" MULT_SYMBOL u"456)"),
     "Prefer to break before image dimensions (1)."},
    {u"abc.png (123" MULT_SYMBOL u"456)", 8, 15,
     BIDIFY(u"abc.png ", u"(123" MULT_SYMBOL u"456)"),
     "Prefer to break before image dimensions (2)."},
    {u"abc.png (123,456)", 15, 15, BIDIFY(u"abc", u".png (123,456)"),
     "Prefer to break before extension if dimensions malformatted."},
    {u"abc.png (123" MULT_SYMBOL u"456)", 15, 9,
     BIDIFY(u"abc.png ", u"(123" MULT_SYMBOL u"456)"),
     "Force break before image dimensions."},
    {u"abc.png (123" MULT_SYMBOL u"456)", 7, 15,
     BIDIFY(u"abc", u".png (123" MULT_SYMBOL u"456)"),
     "Force break before extension."},
};

class TabHoverCardBubbleViewFilenameEliderElideImplTest
    : public TabHoverCardBubbleViewFilenameEliderTest,
      public testing::TestWithParam<ElideImplTestParams> {};

INSTANTIATE_TEST_SUITE_P(,
                         TabHoverCardBubbleViewFilenameEliderElideImplTest,
                         testing::ValuesIn(kElideImplTestParams));

TEST_P(TabHoverCardBubbleViewFilenameEliderElideImplTest, ElideImpl) {
  const ElideImplTestParams& params = GetParam();
  EXPECT_EQ(std::u16string(params.expected),
            ElideImpl(params.text, params.max_first_line_length,
                      params.max_second_line_length));
}

struct ElideTestParams {
  const char16_t* const text;
  const int chars_per_line;
  const size_t expected_first_line_length;
  const size_t expected_second_line_length;
  const char16_t* const elided;
  const char* const comment;
};

void PrintTo(const ElideTestParams& params, ::std::ostream* os) {
  *os << params.comment << " (\"" << params.text << "\", "
      << params.chars_per_line << ")";
}

const ElideTestParams kElideTestParams[]{
    {u"", 1, 0, 0, u"", "Empty string results in zero-length lines."},
    {u"abc", 3, 3, 3, u"abc",
     "Length equal to width gives full length for each line."},
    {u"abc", 4, 3, 3, u"abc",
     "Length less than width gives full length for each line."},
    {u"abcde", 3, 3, 2, u"abc\nde",
     "Maximum length of lines without ellipsis results in perfect match."},
    {u"abcdef", 3, 3, 2, ELLIPSIZE(u"abc", u"ef"),
     "Length equal to width still leaves space for ellipsis on second line."},
    {u"abcdefg", 3, 3, 2, ELLIPSIZE(u"abc", u"fg"),
     "Length much greater than width still leaves space for ellipsis on second "
     "line."},
    {u"abc", 0, 0, 0, u"", "No available width results in zero length."},
    {u"abco" COMBINING_CIRCUMFLEX u"efg", 4, 5, 3,
     "abco" COMBINING_CIRCUMFLEX u"\nefg",
     "First line ends with combining character."},
    {u"abco" COMBINING_CIRCUMFLEX u"efg", 3, 3, 2, ELLIPSIZE(u"abc", u"fg"),
     "Combining character fully elided between lines."},
    {u"abc" MAN_EMOJI u"efg", 4, 5, 3, u"abc" MAN_EMOJI u"\nefg",
     "First line ends with four-byte emoji."},
    {u"abc" MAN_EMOJI u"efg", 3, 3, 2, ELLIPSIZE(u"abc", u"fg"),
     "Four-byte emoji fully elided between lines."},
    {u"abc" MAN_EMOJI MAN_EMOJI MAN_EMOJI u"efg", 3, 3, 2,
     ELLIPSIZE(u"abc", u"fg"), "Elide around a sequence of emoji."},
    {u"abc" MAN_EMOJI MAN_EMOJI MAN_EMOJI u"efg", 4, 5, 3,
     ELLIPSIZE(u"abc" MAN_EMOJI, u"efg"), "Elide around a sequence of emoji."},
    {u"abc" MAN_EMOJI MAN_EMOJI MAN_EMOJI u"efg", 5, 7, 5,
     u"abc" MAN_EMOJI MAN_EMOJI u"\n" MAN_EMOJI u"efg",
     "Line break in middle of sequence."},
    {u"abc" MEDICAL_SYMBOL_EMOJI u"efg", 4, 5, 3,
     u"abc" MEDICAL_SYMBOL_EMOJI u"\nefg",
     "First line ends with two-character emoji."},
    {u"abc" MEDICAL_SYMBOL_EMOJI u"efg", 3, 3, 2, ELLIPSIZE(u"abc", u"fg"),
     "Two-character emoji fully elided between lines."},
    {u"abc" MAN_EMOJI MEDIUM_SKIN_TONE_MODIFIER u"efg", 4, 7, 3,
     u"abc" MAN_EMOJI MEDIUM_SKIN_TONE_MODIFIER u"\nefg",
     "First line ends with modified emoji."},
    {u"abc" MAN_EMOJI MEDIUM_SKIN_TONE_MODIFIER u"efg", 3, 3, 2,
     ELLIPSIZE(u"abc", u"fg"), "Modified emoji fully elided between lines."},
    {u"abc" MALE_HEALTH_WORKER_MEDIUM_SKIN_TONE u"efg", 4, 10, 3,
     u"abc" MALE_HEALTH_WORKER_MEDIUM_SKIN_TONE u"\nefg",
     "First line ends with joined emoji, full string returned."},
    {u"abc" MALE_HEALTH_WORKER_MEDIUM_SKIN_TONE u"efgh", 4, 10, 3,
     ELLIPSIZE(u"abc" MALE_HEALTH_WORKER_MEDIUM_SKIN_TONE, u"fgh"),
     "First line ends with joined emoji, string cut in middle (1)."},
    {u"abc" MALE_HEALTH_WORKER_MEDIUM_SKIN_TONE u"defghi", 5, 11, 4,
     ELLIPSIZE(u"abc" MALE_HEALTH_WORKER_MEDIUM_SKIN_TONE u"d", u"fghi"),
     "First line ends with joined emoji, string cut in middle (2)."},
    {u"abc" MALE_HEALTH_WORKER_MEDIUM_SKIN_TONE u"efg", 3, 3, 2,
     ELLIPSIZE(u"abc", u"fg"), "Joined emoji fully elided between lines."},
    {u"abcde" MALE_HEALTH_WORKER_MEDIUM_SKIN_TONE
         MALE_HEALTH_WORKER_MEDIUM_SKIN_TONE u"fg",
     4, 4, 9, ELLIPSIZE(u"abcd", MALE_HEALTH_WORKER_MEDIUM_SKIN_TONE u"fg"),
     "Joined emoji in sequence; first emoji is elided but not second."},
    // These test the combined function of the Elide() method, including the
    // intelligent overlapping and positioning of lines and extensions.
    {u"abcdef", 5, 5, 4, u"abcde\nf", "Wrap at last possible location."},
    {u"abcde.fgh", 6, 6, 5, BIDIFY(u"abcde", u".fgh"),
     "Entire extension placed on second line (1)."},
    {u"abcde.fgh", 5, 5, 4, BIDIFY(u"abcde", u".fgh"),
     "Entire extension placed on second line (2)."},
    {u"abc.fgh", 4, 4, 3, u"abc.\nfgh", "Force break after period."},
    {u"ab.c (1" MULT_SYMBOL u"2)", 9, 9, 8,
     BIDIFY(u"ab.c ", u"(1" MULT_SYMBOL u"2)"),
     "Prefer to break before dimensions."},
    {u"a.b (1" MULT_SYMBOL u"2)", 8, 8, 7,
     BIDIFY(u"a.b ", u"(1" MULT_SYMBOL u"2)"),
     "Force break before dimensions."},
    {u"abcdefghij.png (1" MULT_SYMBOL u"2)", 13, 13, 12,
     BIDIFY(u"abcdefghij", u".png (1" MULT_SYMBOL u"2)"),
     "Force break before extension."},
};

class TabHoverCardBubbleViewFilenameEliderGetLineLengthsTest
    : public TabHoverCardBubbleViewFilenameEliderTest,
      public testing::TestWithParam<ElideTestParams> {};

INSTANTIATE_TEST_SUITE_P(,
                         TabHoverCardBubbleViewFilenameEliderGetLineLengthsTest,
                         testing::ValuesIn(kElideTestParams));

TEST_P(TabHoverCardBubbleViewFilenameEliderGetLineLengthsTest, GetLineLengths) {
  const ElideTestParams& params = GetParam();
  auto result = GetLineLengths(params.text, params.chars_per_line);
  EXPECT_EQ(params.expected_first_line_length, result.first)
      << "Text length is " << std::u16string(params.text).length();
  EXPECT_EQ(params.expected_second_line_length, result.second);
}

TEST_P(TabHoverCardBubbleViewFilenameEliderGetLineLengthsTest, Elide) {
  const ElideTestParams& params = GetParam();
  FilenameElider elider(CreateRenderText(std::u16string()));
  EXPECT_EQ(std::u16string(params.elided),
            elider.Elide(params.text, GetTextRect(params.chars_per_line)));
}

struct FindImageDimensionsTestParams {
  const char16_t* const text;
  const std::u16string::size_type expected;
  const char* const comment;
};

void PrintTo(const FindImageDimensionsTestParams& params, ::std::ostream* os) {
  *os << params.comment << " (\"" << params.text << "\")";
}

const FindImageDimensionsTestParams kFindImageDimensionsTestParams[]{
    {u"", std::u16string::npos, "Empty string has no dimensions."},
    {u"(", std::u16string::npos, "Single open paren has no dimensions."},
    {u"a (", std::u16string::npos, "Single open paren has no dimensions. (2)"},
    {u"a ()", std::u16string::npos, "Just parens has no dimensions."},
    {u"a (" MULT_SYMBOL u")", std::u16string::npos,
     "No numbers has no dimensions."},
    {u"a (1" MULT_SYMBOL u")", std::u16string::npos,
     "Missing height has no dimensions (1)."},
    {u"a (1234" MULT_SYMBOL u")", std::u16string::npos,
     "Missing height has no dimensions (2)."},
    {u"a (" MULT_SYMBOL u"1)", std::u16string::npos,
     "Missing width has no dimensions (1)."},
    {u"a (" MULT_SYMBOL u"1234)", std::u16string::npos,
     "Missing width has no dimensions (2)."},
    {u"a(1" MULT_SYMBOL u"1)", std::u16string::npos,
     "Missing whitespace has no dimensions."},
    {u"a (1" MULT_SYMBOL u"1", std::u16string::npos,
     "Missing end paren has dimensions."},
    {u"a (1234)", std::u16string::npos, "Missing x has no dimensions."},
    {u"a (1" MULT_SYMBOL u"4)", 2U, "Single digits finds dimensions."},
    {u"a (123" MULT_SYMBOL u"456)", 2U, "Multiple digits finds dimensions."},
    {u"a (123 " MULT_SYMBOL u"456)", std::u16string::npos,
     "Extra whitespace has no dimensions (1)."},
    {u"a (123" MULT_SYMBOL u" 456)", std::u16string::npos,
     "Extra whitespace has no dimensions (2)."},
    {u"a (123 " MULT_SYMBOL u" 456)", std::u16string::npos,
     "Extra whitespace has no dimensions (3)."},
    {u"a (1234567890" MULT_SYMBOL u"1234567890)", 2U,
     "All digits finds dimensions."},
    {u"abc def ghi 12345 () xxxxx foo (123" MULT_SYMBOL u"456)", 31U,
     "Long filename finds dimensions."},
    {u"a (123" MULT_SYMBOL u"456) ", std::u16string::npos,
     "Extra space at end has no dimensions."},
    {u"a (123" MULT_SYMBOL u"456)x", std::u16string::npos,
     "Extra characters at end has no dimensions (1)."},
    {u"a (123" MULT_SYMBOL u"456)(", std::u16string::npos,
     "Extra characters at end has no dimensions (2)."},
};

class TabHoverCardBubbleViewFilenameEliderFindImageDimensionsTest
    : public TabHoverCardBubbleViewFilenameEliderTest,
      public testing::TestWithParam<FindImageDimensionsTestParams> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    TabHoverCardBubbleViewFilenameEliderFindImageDimensionsTest,
    testing::ValuesIn(kFindImageDimensionsTestParams));

TEST_P(TabHoverCardBubbleViewFilenameEliderFindImageDimensionsTest,
       FindImageDimensions) {
  const FindImageDimensionsTestParams& params = GetParam();
  EXPECT_EQ(params.expected, FilenameElider::FindImageDimensions(params.text));
}
