// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_table_detector.h"

#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include "pdf/accessibility_structs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace pdf {

namespace {

// Returns a line segment from (`x0`, `y0`) to (`x1`, `y1`).
chrome_pdf::AccessibilityLineSegment CreateSegment(float x0,
                                                   float y0,
                                                   float x1,
                                                   float y1) {
  return {gfx::PointF(x0, y0), gfx::PointF(x1, y1)};
}

// Returns a horizontal ruling line at `y` from `x_start` to `x_end`.
RulingLine CreateHLine(float y, float x_start, float x_end) {
  return {gfx::PointF(x_start, y), gfx::PointF(x_end, y),
          /*is_horizontal=*/true};
}

// Returns a vertical ruling line at `x` from `y_start` to `y_end`.
RulingLine CreateVLine(float x, float y_start, float y_end) {
  return {gfx::PointF(x, y_start), gfx::PointF(x, y_end),
          /*is_horizontal=*/false};
}

// Returns characters for the BMP-only `text`, each `char_width` px wide.
std::vector<chrome_pdf::AccessibilityCharInfo> CreateChars(
    std::u16string_view text,
    double char_width) {
  std::vector<chrome_pdf::AccessibilityCharInfo> chars;
  chars.reserve(text.size());
  for (char16_t c : text) {
    chars.push_back({static_cast<uint32_t>(c), char_width});
  }
  return chars;
}

// Returns a left-to-right text run.
chrome_pdf::AccessibilityTextRunInfo CreateTextRun(uint32_t start_index,
                                                   uint32_t len,
                                                   const gfx::RectF& bounds) {
  chrome_pdf::AccessibilityTextRunInfo run;
  run.start_index = start_index;
  run.len = len;
  run.bounds = bounds;
  run.direction = chrome_pdf::AccessibilityTextDirection::kLeftToRight;
  return run;
}

// Expects `run` to start at `start_index` with `len` characters and `bounds`.
void ExpectTextRun(const chrome_pdf::AccessibilityTextRunInfo& run,
                   uint32_t start_index,
                   uint32_t len,
                   const gfx::RectF& bounds) {
  EXPECT_EQ(start_index, run.start_index);
  EXPECT_EQ(len, run.len);
  EXPECT_EQ(bounds, run.bounds);
}

}  // namespace

TEST(PdfAccessibilityTableDetectorTest, ExtractRulingLinesEmpty) {
  EXPECT_TRUE(ExtractRulingLines({}, {}).empty());
}

TEST(PdfAccessibilityTableDetectorTest, ExtractRulingLinesFiltersSegments) {
  const std::vector<chrome_pdf::AccessibilityLineSegment> segments = {
      // Horizontal.
      CreateSegment(10, 20, 50, 20),
      // Vertical with reversed endpoints.
      CreateSegment(30, 40, 30, 10),
      // Nearly horizontal, within 1.5 px. Snapped to its centerline.
      CreateSegment(10, 100, 50, 101),
      // Exactly the minimum length of 8 px.
      CreateSegment(10, 300, 18, 300),
      // Diagonal.
      CreateSegment(10, 10, 40, 30),
      // Too short.
      CreateSegment(10, 200, 17, 200),
      // Too slanted.
      CreateSegment(10, 400, 50, 402),
  };

  EXPECT_THAT(
      ExtractRulingLines(segments, {}),
      testing::ElementsAre(CreateHLine(20, 10, 50), CreateHLine(100.5f, 10, 50),
                           CreateHLine(300, 10, 18), CreateVLine(30, 10, 40)));
}

TEST(PdfAccessibilityTableDetectorTest, ExtractRulingLinesMergesCollinear) {
  const std::vector<chrome_pdf::AccessibilityLineSegment> segments = {
      // Overlapping.
      CreateSegment(0, 0, 60, 0),
      CreateSegment(40, 0, 100, 0),
      // Contained.
      CreateSegment(0, 20, 100, 20),
      CreateSegment(30, 20, 60, 20),
      // Separated by a 2 px gap.
      CreateSegment(0, 40, 50, 40),
      CreateSegment(52, 40, 100, 40),
      // Separated by a 3 px gap, so not merged.
      CreateSegment(0, 60, 50, 60),
      CreateSegment(53, 60, 100, 60),
      // Touching.
      CreateSegment(0, 80, 50, 80),
      CreateSegment(50, 80, 100, 80),
      // Offset by 1 px perpendicular to the line. Merged at the smaller y.
      CreateSegment(0, 101, 50, 101),
      CreateSegment(50, 100, 100, 100),
      // Overlapping vertical segments.
      CreateSegment(200, 0, 200, 50),
      CreateSegment(200, 30, 200, 90),
  };

  EXPECT_THAT(
      ExtractRulingLines(segments, {}),
      testing::ElementsAre(CreateHLine(0, 0, 100), CreateHLine(20, 0, 100),
                           CreateHLine(40, 0, 100), CreateHLine(60, 0, 50),
                           CreateHLine(60, 53, 100), CreateHLine(80, 0, 100),
                           CreateHLine(100, 0, 100), CreateVLine(200, 0, 90)));
}

TEST(PdfAccessibilityTableDetectorTest,
     ExtractRulingLinesDoesNotChainSteppedOffsets) {
  // Each segment is within 2 px of the previous one, but only the first two
  // and the last two are within 2 px of each other.
  const std::vector<chrome_pdf::AccessibilityLineSegment> segments = {
      CreateSegment(0, 0, 100, 0),
      CreateSegment(0, 1.5f, 100, 1.5f),
      CreateSegment(0, 3, 100, 3),
      CreateSegment(0, 4.5f, 100, 4.5f),
  };

  EXPECT_THAT(
      ExtractRulingLines(segments, {}),
      testing::ElementsAre(CreateHLine(0, 0, 100), CreateHLine(3, 0, 100)));
}

TEST(PdfAccessibilityTableDetectorTest, ExtractRulingLinesFromThinFilledRects) {
  constexpr std::array filled_rects = {
      // Thin horizontal rectangle.
      gfx::RectF(10, 20, 100, 1),
      // Horizontal rectangle exactly at the maximum thickness of 4 px.
      gfx::RectF(10, 50, 100, 4),
      // Thin vertical rectangle.
      gfx::RectF(200, 10, 2, 50),
      // Too thick.
      gfx::RectF(10, 100, 100, 5),
      // Too short.
      gfx::RectF(10, 150, 7, 1),
  };
  // A stroked segment continuing the first rectangle's centerline.
  const std::vector<chrome_pdf::AccessibilityLineSegment> segments = {
      CreateSegment(110, 20.5f, 150, 20.5f),
  };

  EXPECT_THAT(
      ExtractRulingLines(segments, filled_rects),
      testing::ElementsAre(CreateHLine(20.5f, 10, 150),
                           CreateHLine(52, 10, 110), CreateVLine(201, 10, 60)));
}

// "Left Right" with 10 px characters from x = 100, so the space spans x = 140
// to 150, followed by "Next" on another line.
class PdfAccessibilityTableDetectorSplitTest : public testing::Test {
 protected:
  PdfAccessibilityTableDetectorSplitTest()
      : chars_(CreateChars(u"Left RightNext", 10)),
        text_runs_({CreateTextRun(0, 10, gfx::RectF(100, 100, 100, 12)),
                    CreateTextRun(10, 4, gfx::RectF(100, 200, 40, 12))}) {}

  bool Split(
      const std::vector<chrome_pdf::AccessibilityLineSegment>& segments) {
    SplitTextRunsResult result = SplitTextRunsAtVerticalDividers(
        ExtractRulingLines(segments, {}), chars_, text_runs_);
    if (result.text_runs.empty()) {
      return false;
    }
    text_runs_ = std::move(result.text_runs);
    RemapTextRunIndices(result.first_split_run_indices, page_objects_);
    return true;
  }

  std::vector<chrome_pdf::AccessibilityCharInfo> chars_;
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs_;
  chrome_pdf::AccessibilityPageObjects page_objects_;
};

TEST_F(PdfAccessibilityTableDetectorSplitTest, SplitsAtVerticalDivider) {
  page_objects_.links.push_back({/*url=*/"",
                                 /*index_in_page=*/0,
                                 gfx::RectF(),
                                 {.index = 0, .count = 1}});
  page_objects_.highlights.push_back({/*note_text=*/"",
                                      /*index_in_page=*/0,
                                      /*color=*/0,
                                      gfx::RectF(),
                                      {.index = 1, .count = 1}});
  page_objects_.graphics.push_back({.text_range = {.index = 0, .count = 2}});
  page_objects_.images.push_back({/*alt_text=*/"", /*text_run_index=*/2,
                                  gfx::RectF(), /*page_object_index=*/0});

  ASSERT_TRUE(Split({CreateSegment(145, 95, 145, 117)}));

  // The trailing space stays with "Left ", but its bounds end at the space.
  ASSERT_EQ(3u, text_runs_.size());
  ExpectTextRun(text_runs_[0], 0, 5, gfx::RectF(100, 100, 40, 12));
  ExpectTextRun(text_runs_[1], 5, 5, gfx::RectF(150, 100, 50, 12));
  ExpectTextRun(text_runs_[2], 10, 4, gfx::RectF(100, 200, 40, 12));

  EXPECT_EQ(0u, page_objects_.links[0].text_range.index);
  EXPECT_EQ(2u, page_objects_.links[0].text_range.count);
  EXPECT_EQ(2u, page_objects_.highlights[0].text_range.index);
  EXPECT_EQ(1u, page_objects_.highlights[0].text_range.count);
  EXPECT_EQ(0u, page_objects_.graphics[0].text_range.index);
  EXPECT_EQ(3u, page_objects_.graphics[0].text_range.count);
  EXPECT_EQ(3u, page_objects_.images[0].text_run_index);
}

TEST_F(PdfAccessibilityTableDetectorSplitTest, SplitsAtMultipleDividers) {
  chars_ = CreateChars(u"A B C", 10);
  text_runs_ = {CreateTextRun(0, 5, gfx::RectF(0, 0, 50, 12))};

  ASSERT_TRUE(
      Split({CreateSegment(15, 0, 15, 12), CreateSegment(35, 0, 35, 12)}));

  ASSERT_EQ(3u, text_runs_.size());
  ExpectTextRun(text_runs_[0], 0, 2, gfx::RectF(0, 0, 10, 12));
  ExpectTextRun(text_runs_[1], 2, 2, gfx::RectF(20, 0, 10, 12));
  ExpectTextRun(text_runs_[2], 4, 1, gfx::RectF(40, 0, 10, 12));
}

TEST_F(PdfAccessibilityTableDetectorSplitTest, NoSplitWithoutVerticalLines) {
  ASSERT_EQ(2u, text_runs_.size());
  EXPECT_FALSE(Split({CreateSegment(100, 112, 200, 112)}));
  EXPECT_EQ(2u, text_runs_.size());
}

TEST_F(PdfAccessibilityTableDetectorSplitTest, NoSplitAtLineCrossingText) {
  // A "W" below the gap, which the line runs through, so the line is more
  // likely a chart gridline than a column divider.
  chars_ = CreateChars(u"Left RightW", 10);
  text_runs_ = {CreateTextRun(0, 10, gfx::RectF(100, 100, 100, 12)),
                CreateTextRun(10, 1, gfx::RectF(140, 120, 10, 12))};

  EXPECT_FALSE(Split({CreateSegment(145, 95, 145, 135)}));
  EXPECT_EQ(2u, text_runs_.size());
}

TEST_F(PdfAccessibilityTableDetectorSplitTest, SplitsAtLineEndingAtTextEdges) {
  // A "W" above and below the gap. The line ends at the bottom edge of the
  // upper "W" and the top edge of the lower "W", so it crosses neither.
  chars_ = CreateChars(u"WLeft RightW", 10);
  text_runs_ = {CreateTextRun(0, 1, gfx::RectF(140, 80, 10, 12)),
                CreateTextRun(1, 10, gfx::RectF(100, 100, 100, 12)),
                CreateTextRun(11, 1, gfx::RectF(140, 120, 10, 12))};

  ASSERT_TRUE(Split({CreateSegment(145, 92, 145, 120)}));

  ASSERT_EQ(4u, text_runs_.size());
  ExpectTextRun(text_runs_[0], 0, 1, gfx::RectF(140, 80, 10, 12));
  ExpectTextRun(text_runs_[1], 1, 5, gfx::RectF(100, 100, 40, 12));
  ExpectTextRun(text_runs_[2], 6, 5, gfx::RectF(150, 100, 50, 12));
  ExpectTextRun(text_runs_[3], 11, 1, gfx::RectF(140, 120, 10, 12));
}

TEST_F(PdfAccessibilityTableDetectorSplitTest,
       SplitsAtVerticalDividerWithCombiningMarks) {
  // "Cómo Estás", with each accent as a combining mark inside its base letter,
  // so the letter's 10 px is shared between the two characters.
  chars_ = CreateChars(u"Co\u0301mo Esta\u0301s", 10);
  chars_[1].char_width = 4;
  chars_[2].char_width = 6;
  chars_[9].char_width = 4;
  chars_[10].char_width = 6;
  text_runs_ = {CreateTextRun(0, 12, gfx::RectF(100, 100, 100, 12))};

  ASSERT_TRUE(Split({CreateSegment(145, 95, 145, 117)}));

  ASSERT_EQ(2u, text_runs_.size());
  ExpectTextRun(text_runs_[0], 0, 6, gfx::RectF(100, 100, 40, 12));
  ExpectTextRun(text_runs_[1], 6, 6, gfx::RectF(150, 100, 50, 12));
}

TEST_F(PdfAccessibilityTableDetectorSplitTest,
       NoSplitAtLineCrossingCombiningMark) {
  // An "ó" below the gap, as an "o" followed by a combining accent that starts
  // left of the "o". That gives the "o" a negative width, but the line still
  // runs through the accent's box.
  chars_ = CreateChars(u"Left Righto\u0301", 10);
  chars_[10].char_width = -2;
  chars_[11].char_width = 12;
  text_runs_ = {CreateTextRun(0, 10, gfx::RectF(100, 100, 100, 12)),
                CreateTextRun(10, 2, gfx::RectF(140, 120, 10, 12))};

  EXPECT_FALSE(Split({CreateSegment(145, 95, 145, 135)}));
  EXPECT_EQ(2u, text_runs_.size());
}

TEST_F(PdfAccessibilityTableDetectorSplitTest, SplitsThaiTextWithStackedMarks) {
  // "ภาษา ที่", with a space between the cells. Thai doesn't put spaces between
  // words, so this space would come from PDFium generating one in a wide enough
  // gap. The two marks in "ที่" sit over "ท", so they share its 10 px.
  chars_ = CreateChars(u"\u0e20\u0e32\u0e29\u0e32 \u0e17\u0e35\u0e48", 10);
  chars_[5].char_width = 2;
  chars_[6].char_width = 0;
  chars_[7].char_width = 8;
  text_runs_ = {CreateTextRun(0, 8, gfx::RectF(100, 100, 60, 12))};

  ASSERT_TRUE(Split({CreateSegment(145, 95, 145, 117)}));

  ASSERT_EQ(2u, text_runs_.size());
  ExpectTextRun(text_runs_[0], 0, 5, gfx::RectF(100, 100, 40, 12));
  ExpectTextRun(text_runs_[1], 5, 3, gfx::RectF(150, 100, 10, 12));
}

TEST_F(PdfAccessibilityTableDetectorSplitTest, NoSplitAtNarrowGap) {
  ASSERT_EQ(14u, chars_.size());
  ASSERT_EQ(2u, text_runs_.size());

  // A 2 px space is too narrow for table cell padding.
  chars_[4].char_width = 2;

  EXPECT_FALSE(Split({CreateSegment(141, 95, 141, 117)}));
  EXPECT_EQ(2u, text_runs_.size());
}

TEST_F(PdfAccessibilityTableDetectorSplitTest, NoSplitAtNarrowGapForTallText) {
  ASSERT_EQ(14u, chars_.size());
  ASSERT_EQ(2u, text_runs_.size());

  // An 8 px space clears the 3 px floor, but is too narrow for 40 px text.
  chars_[4].char_width = 8;
  text_runs_[0].bounds.set_height(40);

  EXPECT_FALSE(Split({CreateSegment(144, 95, 144, 145)}));
  EXPECT_EQ(2u, text_runs_.size());
}

TEST_F(PdfAccessibilityTableDetectorSplitTest, NoSplitAtShortLine) {
  ASSERT_EQ(2u, text_runs_.size());

  // The line covers less than half of the text height.
  EXPECT_FALSE(Split({CreateSegment(145, 90, 145, 104)}));
  EXPECT_EQ(2u, text_runs_.size());
}

TEST_F(PdfAccessibilityTableDetectorSplitTest, NoSplitAtLineOutsideRun) {
  ASSERT_EQ(2u, text_runs_.size());

  // The line is in the gap's x range, but above the text.
  EXPECT_FALSE(Split({CreateSegment(145, 50, 145, 90)}));
  EXPECT_EQ(2u, text_runs_.size());
}

TEST_F(PdfAccessibilityTableDetectorSplitTest, NoSplitRightToLeft) {
  ASSERT_EQ(2u, text_runs_.size());
  text_runs_[0].direction =
      chrome_pdf::AccessibilityTextDirection::kRightToLeft;

  EXPECT_FALSE(Split({CreateSegment(145, 95, 145, 117)}));
  EXPECT_EQ(2u, text_runs_.size());
}

}  // namespace pdf
