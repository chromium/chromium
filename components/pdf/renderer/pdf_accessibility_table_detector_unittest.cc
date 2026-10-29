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

// Returns characters for the ASCII `text`, each `char_width` px wide.
std::vector<chrome_pdf::AccessibilityCharInfo> CreateChars(
    std::string_view text,
    double char_width) {
  std::vector<chrome_pdf::AccessibilityCharInfo> chars;
  chars.reserve(text.size());
  for (char c : text) {
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
      : chars_(CreateChars("Left RightNext", 10)),
        text_runs_({CreateTextRun(0, 10, gfx::RectF(100, 100, 100, 12)),
                    CreateTextRun(10, 4, gfx::RectF(100, 200, 40, 12))}) {}

  bool Split(
      const std::vector<chrome_pdf::AccessibilityLineSegment>& segments) {
    std::vector<chrome_pdf::AccessibilityTextRunInfo> split_runs =
        SplitTextRunsAtVerticalDividers(ExtractRulingLines(segments, {}),
                                        chars_, text_runs_);
    if (split_runs.empty()) {
      return false;
    }
    text_runs_ = std::move(split_runs);
    return true;
  }

  std::vector<chrome_pdf::AccessibilityCharInfo> chars_;
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs_;
};

TEST_F(PdfAccessibilityTableDetectorSplitTest, SplitsAtVerticalDivider) {
  ASSERT_TRUE(Split({CreateSegment(145, 95, 145, 117)}));

  // The trailing space stays with "Left ", but its bounds end at the space.
  ASSERT_EQ(3u, text_runs_.size());
  ExpectTextRun(text_runs_[0], 0, 5, gfx::RectF(100, 100, 40, 12));
  ExpectTextRun(text_runs_[1], 5, 5, gfx::RectF(150, 100, 50, 12));
  ExpectTextRun(text_runs_[2], 10, 4, gfx::RectF(100, 200, 40, 12));
}

TEST_F(PdfAccessibilityTableDetectorSplitTest, SplitsAtMultipleDividers) {
  chars_ = CreateChars("A B C", 10);
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
