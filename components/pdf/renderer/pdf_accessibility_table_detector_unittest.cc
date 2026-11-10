// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_table_detector.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/extend.h"
#include "base/containers/span.h"
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

// Boundaries of grids with 30 px cells.
constexpr float kTwoCellBoundaries[] = {0, 30, 60};
constexpr float kThreeCellBoundaries[] = {0, 30, 60, 90};
constexpr float kFourCellBoundaries[] = {0, 30, 60, 90, 120};
constexpr float kFiveCellBoundaries[] = {0, 30, 60, 90, 120, 150};

// Returns full-span ruling lines for a grid with the given column (`xs`) and
// row (`ys`) boundaries.
std::vector<RulingLine> CreateGridLines(base::span<const float> xs,
                                        base::span<const float> ys) {
  std::vector<RulingLine> lines;
  lines.reserve(xs.size() + ys.size());
  for (float y : ys) {
    lines.push_back(CreateHLine(y, xs.front(), xs.back()));
  }
  for (float x : xs) {
    lines.push_back(CreateVLine(x, ys.front(), ys.back()));
  }
  return lines;
}

// Returns a text run with the given bounds and optional text direction.
chrome_pdf::AccessibilityTextRunInfo CreateTextRun(
    const gfx::RectF& bounds,
    chrome_pdf::AccessibilityTextDirection direction =
        chrome_pdf::AccessibilityTextDirection::kNone) {
  chrome_pdf::AccessibilityTextRunInfo run;
  run.bounds = bounds;
  run.direction = direction;
  return run;
}

// Returns one text run per listed {row, col} cell of the grid with boundaries
// `xs` and `ys`, inset 5 px from the cell edges, in the listed order. Rows must
// be less than `ys.size() - 1`, columns less than `xs.size() - 1`, and cells
// larger than 10 px in both dimensions.
std::vector<chrome_pdf::AccessibilityTextRunInfo> CreateRunsInCells(
    base::span<const float> xs,
    base::span<const float> ys,
    std::vector<std::pair<size_t, size_t>> cells,
    chrome_pdf::AccessibilityTextDirection direction =
        chrome_pdf::AccessibilityTextDirection::kNone) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.reserve(cells.size());
  for (const auto& [r, c] : cells) {
    gfx::RectF bounds(xs[c], ys[r], xs[c + 1] - xs[c], ys[r + 1] - ys[r]);
    bounds.Inset(5);
    CHECK(!bounds.IsEmpty());
    text_runs.push_back(CreateTextRun(bounds, direction));
  }
  return text_runs;
}

// Returns one text run per cell of the grid with boundaries `xs` and `ys`, in
// row-major order.
std::vector<chrome_pdf::AccessibilityTextRunInfo> CreateRunsInAllCells(
    base::span<const float> xs,
    base::span<const float> ys) {
  std::vector<std::pair<size_t, size_t>> cells;
  cells.reserve((ys.size() - 1) * (xs.size() - 1));
  for (size_t r = 0; r < ys.size() - 1; ++r) {
    for (size_t c = 0; c < xs.size() - 1; ++c) {
      cells.emplace_back(r, c);
    }
  }
  return CreateRunsInCells(xs, ys, std::move(cells));
}

// Ruling lines and text runs to pass to `DetectGridTables()`.
struct GridInput {
  std::vector<RulingLine> ruling_lines;
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
};

// Returns full-span ruling lines for the grid with boundaries `xs` and `ys`,
// and one text run per cell, in row-major order.
GridInput CreateGridWithRunInEveryCell(base::span<const float> xs,
                                       base::span<const float> ys) {
  return {CreateGridLines(xs, ys), CreateRunsInAllCells(xs, ys)};
}

// Returns full-span ruling lines for the grid with boundaries `xs` and `ys`,
// and one text run with `direction` per listed {row, col} cell, in the listed
// order.
GridInput CreateGridWithRunsInCells(
    base::span<const float> xs,
    base::span<const float> ys,
    std::vector<std::pair<size_t, size_t>> cells,
    chrome_pdf::AccessibilityTextDirection direction =
        chrome_pdf::AccessibilityTextDirection::kNone) {
  return {CreateGridLines(xs, ys),
          CreateRunsInCells(xs, ys, std::move(cells), direction)};
}

// Boundaries of a 2-row, 3-column grid with 120 px cells, and of a 4-row,
// 5-column grid nested in its top-left cell without touching its lines:
//   +-------------+-------------+-------------+
//   | +-+-+-+-+-+ |             |             |
//   | +-+-+-+-+-+ |             |             |
//   | +-+-+-+-+-+ |             |             |
//   | +-+-+-+-+-+ |             |             |
//   | +-+-+-+-+-+ |             |             |
//   +-------------+-------------+-------------+
//   |             |             |             |
//   +-------------+-------------+-------------+
constexpr float kOuterGridXs[] = {0, 120, 240, 360};
constexpr float kOuterGridYs[] = {0, 120, 240};
constexpr float kNestedGridXs[] = {10, 30, 50, 70, 90, 110};
constexpr float kNestedGridYs[] = {10, 35, 60, 85, 110};

// Returns the ruling lines of the outer and nested grids, ordered as returned
// by `ExtractRulingLines()`: horizontal lines first, then vertical lines
// sorted by x.
std::vector<RulingLine> CreateNestedGridLines() {
  std::vector<RulingLine> lines = CreateGridLines(kOuterGridXs, kOuterGridYs);
  base::Extend(lines, CreateGridLines(kNestedGridXs, kNestedGridYs));
  std::ranges::sort(lines, {}, [](const RulingLine& line) {
    return std::pair(!line.is_horizontal,
                     line.is_horizontal ? line.start.y() : line.start.x());
  });
  return lines;
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

TEST(PdfAccessibilityTableDetectorTest, DetectsGridBoundaries) {
  std::vector<RulingLine> ruling_lines =
      CreateGridLines(kThreeCellBoundaries, kThreeCellBoundaries);

  EXPECT_THAT(
      DetectRulingGrids(ruling_lines),
      testing::ElementsAre(RulingGrid{.row_boundaries = {0, 30, 60, 90},
                                      .column_boundaries = {0, 30, 60, 90}}));
}

TEST(PdfAccessibilityTableDetectorTest, DetectsGridWithMoreColumnsThanRows) {
  constexpr float kRowBoundaries[] = {0, 30, 60};
  std::vector<RulingLine> ruling_lines =
      CreateGridLines(kThreeCellBoundaries, kRowBoundaries);

  EXPECT_THAT(
      DetectRulingGrids(ruling_lines),
      testing::ElementsAre(RulingGrid{.row_boundaries = {0, 30, 60},
                                      .column_boundaries = {0, 30, 60, 90}}));
}

TEST(PdfAccessibilityTableDetectorTest, DetectsGridWithMoreRowsThanColumns) {
  constexpr float kColumnBoundaries[] = {0, 30, 60};
  std::vector<RulingLine> ruling_lines =
      CreateGridLines(kColumnBoundaries, kThreeCellBoundaries);

  EXPECT_THAT(
      DetectRulingGrids(ruling_lines),
      testing::ElementsAre(RulingGrid{.row_boundaries = {0, 30, 60, 90},
                                      .column_boundaries = {0, 30, 60}}));
}

TEST(PdfAccessibilityTableDetectorTest, MergedCellKeepsAllBoundaries) {
  // 3x3 grid where the middle row's first two cells are merged: the vertical
  // line at x = 30 skips y = 30 to 60. Every row still gets every column
  // boundary, and the merged cell spans two grid cells.
  std::vector<RulingLine> ruling_lines = {
      CreateHLine(0, 0, 90),   CreateHLine(30, 0, 90), CreateHLine(60, 0, 90),
      CreateHLine(90, 0, 90),  CreateVLine(0, 0, 90),  CreateVLine(30, 0, 30),
      CreateVLine(30, 60, 90), CreateVLine(60, 0, 90), CreateVLine(90, 0, 90),
  };

  EXPECT_THAT(
      DetectRulingGrids(ruling_lines),
      testing::ElementsAre(RulingGrid{.row_boundaries = {0, 30, 60, 90},
                                      .column_boundaries = {0, 30, 60, 90}}));
}

TEST(PdfAccessibilityTableDetectorTest, RejectsSingleBox) {
  // 1x1 box: R = 1, C = 1.
  std::vector<RulingLine> ruling_lines = {
      CreateHLine(0, 0, 100),
      CreateHLine(50, 0, 100),
      CreateVLine(0, 0, 50),
      CreateVLine(100, 0, 50),
  };

  EXPECT_TRUE(DetectRulingGrids(ruling_lines).empty());
}

TEST(PdfAccessibilityTableDetectorTest, RejectsExcessiveLinesGuardrail) {
  std::vector<RulingLine> ruling_lines;
  // 1,001 horizontal lines.
  for (int i = 0; i <= 1000; ++i) {
    ruling_lines.push_back(CreateHLine(i, 0, 100));
  }
  // 3 vertical lines.
  ruling_lines.push_back(CreateVLine(0, 0, 1000));
  ruling_lines.push_back(CreateVLine(50, 0, 1000));
  ruling_lines.push_back(CreateVLine(100, 0, 1000));

  EXPECT_TRUE(DetectRulingGrids(ruling_lines).empty());
}

TEST(PdfAccessibilityTableDetectorTest, ConnectsLinesWithinTolerance) {
  // Vertical lines that stop 3 px short of the outer horizontals still form one
  // grid.
  std::vector<RulingLine> ruling_lines = {
      CreateHLine(0, 0, 60), CreateHLine(30, 0, 60), CreateHLine(60, 0, 60),
      CreateVLine(0, 3, 57), CreateVLine(30, 3, 57), CreateVLine(60, 3, 57),
  };

  EXPECT_THAT(
      DetectRulingGrids(ruling_lines),
      testing::ElementsAre(RulingGrid{.row_boundaries = {0, 30, 60},
                                      .column_boundaries = {0, 30, 60}}));
}

TEST(PdfAccessibilityTableDetectorTest, DoesNotConnectLinesBeyondTolerance) {
  // With a 4 px gap, no grid.
  std::vector<RulingLine> ruling_lines = {
      CreateHLine(0, 0, 60), CreateHLine(30, 0, 60), CreateHLine(60, 0, 60),
      CreateVLine(0, 4, 56), CreateVLine(30, 4, 56), CreateVLine(60, 4, 56),
  };

  EXPECT_TRUE(DetectRulingGrids(ruling_lines).empty());
}

TEST(PdfAccessibilityTableDetectorTest, MergesNearbyBoundaries) {
  // An interior vertical line split into two pieces at x = 30 and x = 34 gives
  // a single boundary at 32, the mean.
  std::vector<RulingLine> ruling_lines = {
      CreateHLine(0, 0, 60),  CreateHLine(30, 0, 60), CreateHLine(60, 0, 60),
      CreateVLine(0, 0, 60),  CreateVLine(30, 0, 30), CreateVLine(34, 30, 60),
      CreateVLine(60, 0, 60),
  };

  EXPECT_THAT(
      DetectRulingGrids(ruling_lines),
      testing::ElementsAre(RulingGrid{.row_boundaries = {0, 30, 60},
                                      .column_boundaries = {0, 32, 60}}));
}

TEST(PdfAccessibilityTableDetectorTest, SeparatesDisconnectedGrids) {
  // Two grids far apart give two results.
  std::vector<RulingLine> ruling_lines = {
      CreateHLine(0, 0, 60),   CreateHLine(30, 0, 60),
      CreateHLine(60, 0, 60),  CreateHLine(100, 0, 60),
      CreateHLine(130, 0, 60), CreateHLine(160, 0, 60),
      CreateVLine(0, 0, 60),   CreateVLine(0, 100, 160),
      CreateVLine(30, 0, 60),  CreateVLine(30, 100, 160),
      CreateVLine(60, 0, 60),  CreateVLine(60, 100, 160),
  };

  EXPECT_THAT(DetectRulingGrids(ruling_lines),
              testing::UnorderedElementsAre(
                  RulingGrid{.row_boundaries = {0, 30, 60},
                             .column_boundaries = {0, 30, 60}},
                  RulingGrid{.row_boundaries = {100, 130, 160},
                             .column_boundaries = {0, 30, 60}}));
}

TEST(PdfAccessibilityTableDetectorTest, RejectsGridWithOvershootingLines) {
  // 2x2 grid where an interior vertical line extends 35 px past the bottom
  // border:
  //   +----+----+
  //   |    |    |
  //   +----+----+
  //   |    |    |
  //   +----+----+
  //        |
  //        |
  std::vector<RulingLine> ruling_lines =
      CreateGridLines(kTwoCellBoundaries, kTwoCellBoundaries);
  // Horizontal lines come first, so index 4 is the vertical line at x = 30.
  ruling_lines[4] = CreateVLine(30, 0, 95);

  EXPECT_TRUE(DetectRulingGrids(ruling_lines).empty());
}

TEST(PdfAccessibilityTableDetectorTest, Grid3x3WithOneRunPerCell) {
  // 3x3 grid: horizontal lines at y = 0, 30, 60, 90; vertical lines at x = 0,
  // 30, 60, 90.
  const auto [ruling_lines, text_runs] =
      CreateGridWithRunInEveryCell(kThreeCellBoundaries, kThreeCellBoundaries);

  std::vector<HeuristicTable> tables =
      DetectGridTables(text_runs, ruling_lines);
  ASSERT_EQ(tables.size(), 1u);

  const HeuristicTable& table = tables[0];
  EXPECT_EQ(table.bounds, gfx::RectF(0, 0, 90, 90));
  EXPECT_EQ(table.anchor_text_run_index, 0u);
  ASSERT_EQ(table.rows.size(), 3u);

  size_t expected_run_index = 0;
  for (size_t r = 0; r < table.rows.size(); ++r) {
    const HeuristicTableRow& row = table.rows[r];
    EXPECT_EQ(row.bounds, gfx::RectF(0, r * 30, 90, 30));
    ASSERT_EQ(row.cells.size(), 3u);

    for (size_t c = 0; c < row.cells.size(); ++c) {
      const HeuristicTableCell& cell = row.cells[c];
      EXPECT_EQ(cell.bounds, gfx::RectF(c * 30, r * 30, 30, 30));
      ASSERT_EQ(cell.text_run_indices.size(), 1u);
      EXPECT_EQ(cell.text_run_indices[0], expected_run_index++);
    }
  }
}

TEST(PdfAccessibilityTableDetectorTest, Grid2x3WithOneRunPerCell) {
  // 2 rows x 3 columns: horizontal lines at y = 0, 30, 60; vertical lines at
  // x = 0, 30, 60, 90.
  const auto [ruling_lines, text_runs] =
      CreateGridWithRunInEveryCell(kThreeCellBoundaries, kTwoCellBoundaries);

  std::vector<HeuristicTable> tables =
      DetectGridTables(text_runs, ruling_lines);
  ASSERT_EQ(tables.size(), 1u);

  const HeuristicTable& table = tables[0];
  EXPECT_EQ(table.bounds, gfx::RectF(0, 0, 90, 60));
  EXPECT_EQ(table.anchor_text_run_index, 0u);
  ASSERT_EQ(table.rows.size(), 2u);

  size_t expected_run_index = 0;
  for (size_t r = 0; r < table.rows.size(); ++r) {
    const HeuristicTableRow& row = table.rows[r];
    EXPECT_EQ(row.bounds, gfx::RectF(0, r * 30, 90, 30));
    ASSERT_EQ(row.cells.size(), 3u);

    for (size_t c = 0; c < row.cells.size(); ++c) {
      const HeuristicTableCell& cell = row.cells[c];
      EXPECT_EQ(cell.bounds, gfx::RectF(c * 30, r * 30, 30, 30));
      ASSERT_EQ(cell.text_run_indices.size(), 1u);
      EXPECT_EQ(cell.text_run_indices[0], expected_run_index++);
    }
  }
}

TEST(PdfAccessibilityTableDetectorTest, Grid3x2WithOneRunPerCell) {
  // 3 rows x 2 columns: horizontal lines at y = 0, 30, 60, 90; vertical lines
  // at x = 0, 30, 60.
  const auto [ruling_lines, text_runs] =
      CreateGridWithRunInEveryCell(kTwoCellBoundaries, kThreeCellBoundaries);

  std::vector<HeuristicTable> tables =
      DetectGridTables(text_runs, ruling_lines);
  ASSERT_EQ(tables.size(), 1u);

  const HeuristicTable& table = tables[0];
  EXPECT_EQ(table.bounds, gfx::RectF(0, 0, 60, 90));
  EXPECT_EQ(table.anchor_text_run_index, 0u);
  ASSERT_EQ(table.rows.size(), 3u);

  size_t expected_run_index = 0;
  for (size_t r = 0; r < table.rows.size(); ++r) {
    const HeuristicTableRow& row = table.rows[r];
    EXPECT_EQ(row.bounds, gfx::RectF(0, r * 30, 60, 30));
    ASSERT_EQ(row.cells.size(), 2u);

    for (size_t c = 0; c < row.cells.size(); ++c) {
      const HeuristicTableCell& cell = row.cells[c];
      EXPECT_EQ(cell.bounds, gfx::RectF(c * 30, r * 30, 30, 30));
      ASSERT_EQ(cell.text_run_indices.size(), 1u);
      EXPECT_EQ(cell.text_run_indices[0], expected_run_index++);
    }
  }
}

TEST(PdfAccessibilityTableDetectorTest, CellWithTwoRunsAscendingOrder) {
  // 2x2 grid.
  std::vector<RulingLine> ruling_lines =
      CreateGridLines(kTwoCellBoundaries, kTwoCellBoundaries);

  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs = {
      // Cell (0, 0): run 0
      CreateTextRun(gfx::RectF(5, 5, 20, 20)),
      // Cell (0, 1): run 1
      CreateTextRun(gfx::RectF(35, 5, 20, 20)),
      // Cell (1, 0): run 2 and run 3 (two lines of text)
      CreateTextRun(gfx::RectF(5, 33, 20, 8)),
      CreateTextRun(gfx::RectF(5, 45, 20, 8)),
      // Cell (1, 1): run 4
      CreateTextRun(gfx::RectF(35, 35, 20, 20)),
  };

  std::vector<HeuristicTable> tables =
      DetectGridTables(text_runs, ruling_lines);
  ASSERT_EQ(tables.size(), 1u);

  const HeuristicTable& table = tables[0];
  ASSERT_EQ(table.rows.size(), 2u);
  EXPECT_EQ(table.rows[0].cells.size(), 2u);

  const HeuristicTableCell& cell_1_0 = table.rows[1].cells[0];
  ASSERT_EQ(cell_1_0.text_run_indices.size(), 2u);
  EXPECT_EQ(cell_1_0.text_run_indices[0], 2u);
  EXPECT_EQ(cell_1_0.text_run_indices[1], 3u);
}

TEST(PdfAccessibilityTableDetectorTest, RejectsGridWithNoText) {
  std::vector<RulingLine> ruling_lines =
      CreateGridLines(kThreeCellBoundaries, kThreeCellBoundaries);

  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  EXPECT_TRUE(DetectGridTables(text_runs, ruling_lines).empty());
}

TEST(PdfAccessibilityTableDetectorTest, RejectsGridWithRunsCrossingColumns) {
  // 3x3 grid. Two runs (`=`) cross the column line at x = 30, and 8 runs sit
  // in the center cell:
  //   +-----+-----+-----+
  //   |  ===|===  |     |
  //   +-----+-----+-----+
  //   |     | x 8 |     |
  //   +-----+-----+-----+
  //   |  ===|===  |     |
  //   +-----+-----+-----+
  std::vector<RulingLine> ruling_lines =
      CreateGridLines(kThreeCellBoundaries, kThreeCellBoundaries);

  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  // 10 runs total.
  text_runs.reserve(10);
  // 8 normal runs.
  for (int i = 0; i < 8; ++i) {
    text_runs.push_back(CreateTextRun(gfx::RectF(35, 35, 20, 20)));
  }
  // 2 runs that cross the interior vertical line at x = 30 by more than 3 px on
  // each side: bounds x = 20 to 40 (start 20 < 27, end 40 > 33).
  // 2 out of 10 is 20% > 10% threshold.
  text_runs.push_back(CreateTextRun(gfx::RectF(20, 5, 20, 10)));
  text_runs.push_back(CreateTextRun(gfx::RectF(20, 65, 20, 10)));

  EXPECT_TRUE(DetectGridTables(text_runs, ruling_lines).empty());
}

TEST(PdfAccessibilityTableDetectorTest,
     RejectsGridWithPredominantlyVerticalRuns) {
  // 2x2 grid where all 4 cells have vertical text runs (e.g. sideways table).
  auto [ruling_lines, text_runs] = CreateGridWithRunsInCells(
      kTwoCellBoundaries, kTwoCellBoundaries, {{0, 0}, {0, 1}},
      chrome_pdf::AccessibilityTextDirection::kTopToBottom);
  base::Extend(text_runs,
               CreateRunsInCells(
                   kTwoCellBoundaries, kTwoCellBoundaries, {{1, 0}, {1, 1}},
                   chrome_pdf::AccessibilityTextDirection::kBottomToTop));

  EXPECT_TRUE(DetectGridTables(text_runs, ruling_lines).empty());
}

TEST(PdfAccessibilityTableDetectorTest, AcceptsGridWithRotatedHeaders) {
  // 3 rows x 2 columns. Row 0 has rotated (vertical) headers, and rows 1 and 2
  // have horizontal data runs. 2 vertical runs out of 6 total runs (33% <=
  // 50%).
  auto [ruling_lines, text_runs] = CreateGridWithRunsInCells(
      kTwoCellBoundaries, kThreeCellBoundaries, {{0, 0}, {0, 1}},
      chrome_pdf::AccessibilityTextDirection::kTopToBottom);
  base::Extend(
      text_runs,
      CreateRunsInCells(kTwoCellBoundaries, kThreeCellBoundaries,
                        {{1, 0}, {1, 1}, {2, 0}, {2, 1}},
                        chrome_pdf::AccessibilityTextDirection::kLeftToRight));

  std::vector<HeuristicTable> tables =
      DetectGridTables(text_runs, ruling_lines);
  ASSERT_EQ(tables.size(), 1u);
  ASSERT_EQ(tables[0].rows.size(), 3u);
  EXPECT_EQ(tables[0].rows[0].cells.size(), 2u);
}

// In the grid diagrams below, `X` marks a cell with text and `.` an empty cell.

TEST(PdfAccessibilityTableDetectorTest, PrunesEmptyOuterRowsAndColumns) {
  // 4x4 grid. Only the central 2x2 cells have text, so the table is trimmed to
  // them:
  //   . . . .
  //   . X X .
  //   . X X .
  //   . . . .
  const auto [ruling_lines, text_runs] =
      CreateGridWithRunsInCells(kFourCellBoundaries, kFourCellBoundaries,
                                {{1, 1}, {1, 2}, {2, 1}, {2, 2}});

  std::vector<HeuristicTable> tables =
      DetectGridTables(text_runs, ruling_lines);
  ASSERT_EQ(tables.size(), 1u);

  const HeuristicTable& table = tables[0];
  // Pruned down to 2x2.
  EXPECT_EQ(table.bounds, gfx::RectF(30, 30, 60, 60));
  EXPECT_EQ(table.anchor_text_run_index, 0u);
  ASSERT_EQ(table.rows.size(), 2u);
  EXPECT_EQ(table.rows[0].cells.size(), 2u);
  EXPECT_EQ(table.rows[0].cells[0].bounds, gfx::RectF(30, 30, 30, 30));
  EXPECT_EQ(table.rows[1].cells[1].bounds, gfx::RectF(60, 60, 30, 30));
}

TEST(PdfAccessibilityTableDetectorTest, PrunesToPartlyEmptyNonSquareBlock) {
  // 4x4 grid. Trimming the top row and the right two columns leaves a 3x2
  // block with 4 of 6 cells (67%) populated:
  //   . . . .
  //   X X . .
  //   X . . .
  //   . X . .
  const auto [ruling_lines, text_runs] =
      CreateGridWithRunsInCells(kFourCellBoundaries, kFourCellBoundaries,
                                {{1, 0}, {1, 1}, {2, 0}, {3, 1}});

  std::vector<HeuristicTable> tables =
      DetectGridTables(text_runs, ruling_lines);
  ASSERT_EQ(tables.size(), 1u);

  const HeuristicTable& table = tables[0];
  EXPECT_EQ(table.bounds, gfx::RectF(0, 30, 60, 90));
  ASSERT_EQ(table.rows.size(), 3u);
  for (const HeuristicTableRow& row : table.rows) {
    ASSERT_EQ(row.cells.size(), 2u);
  }
  // Empty cells inside the block are kept.
  EXPECT_THAT(table.rows[0].cells[0].text_run_indices,
              testing::ElementsAre(0u));
  EXPECT_THAT(table.rows[0].cells[1].text_run_indices,
              testing::ElementsAre(1u));
  EXPECT_THAT(table.rows[1].cells[0].text_run_indices,
              testing::ElementsAre(2u));
  EXPECT_TRUE(table.rows[1].cells[1].text_run_indices.empty());
  EXPECT_TRUE(table.rows[2].cells[0].text_run_indices.empty());
  EXPECT_THAT(table.rows[2].cells[1].text_run_indices,
              testing::ElementsAre(3u));
}

TEST(PdfAccessibilityTableDetectorTest, AcceptsGridAtMinPopulatedFraction) {
  // 2x2 grid with 2 of 4 cells (50%) populated, the minimum:
  //   X .
  //   . X
  const auto [ruling_lines, text_runs] = CreateGridWithRunsInCells(
      kTwoCellBoundaries, kTwoCellBoundaries, {{0, 0}, {1, 1}});

  std::vector<HeuristicTable> tables =
      DetectGridTables(text_runs, ruling_lines);
  ASSERT_EQ(tables.size(), 1u);
  EXPECT_EQ(tables[0].bounds, gfx::RectF(0, 0, 60, 60));
}

TEST(PdfAccessibilityTableDetectorTest, RejectsSingleRowAfterPruning) {
  // 3 rows x 5 columns. Only two cells in the middle row have text, which trims
  // to 1x2:
  //   . . . . .
  //   . . X X .
  //   . . . . .
  const auto [ruling_lines, text_runs] = CreateGridWithRunsInCells(
      kFiveCellBoundaries, kThreeCellBoundaries, {{1, 2}, {1, 3}});

  EXPECT_TRUE(DetectGridTables(text_runs, ruling_lines).empty());
}

TEST(PdfAccessibilityTableDetectorTest, RejectsSingleColumnAfterPruning) {
  // 4 rows x 3 columns. Only the right column has text, which trims to 4x1:
  //   . . X
  //   . . X
  //   . . X
  //   . . X
  const auto [ruling_lines, text_runs] =
      CreateGridWithRunsInCells(kThreeCellBoundaries, kFourCellBoundaries,
                                {{0, 2}, {1, 2}, {2, 2}, {3, 2}});

  EXPECT_TRUE(DetectGridTables(text_runs, ruling_lines).empty());
}

TEST(PdfAccessibilityTableDetectorTest, RejectsSparselyPopulatedGrid) {
  // 3x3 grid. The corner cells keep the full grid after trimming, but only 2 of
  // 9 cells (22%) are populated:
  //   X . .
  //   . . .
  //   . . X
  const auto [ruling_lines, text_runs] = CreateGridWithRunsInCells(
      kThreeCellBoundaries, kThreeCellBoundaries, {{0, 0}, {2, 2}});

  EXPECT_TRUE(DetectGridTables(text_runs, ruling_lines).empty());
}

TEST(PdfAccessibilityTableDetectorTest,
     RejectsSparseGridWithEveryRowAndColumnPopulated) {
  // 5 rows x 6 columns. Trimming leaves a 3x4 block where every row and column
  // has text, but only 5 of 12 cells (42%) are populated:
  //   . . . . . .
  //   . . X . . .
  //   . X . X . .
  //   . . . X X .
  //   . . . . . .
  constexpr float kSixCellBoundaries[] = {0, 30, 60, 90, 120, 150, 180};
  const auto [ruling_lines, text_runs] =
      CreateGridWithRunsInCells(kSixCellBoundaries, kFiveCellBoundaries,
                                {{1, 2}, {2, 1}, {2, 3}, {3, 3}, {3, 4}});

  EXPECT_TRUE(DetectGridTables(text_runs, ruling_lines).empty());
}

TEST(PdfAccessibilityTableDetectorTest, TwoSeparateGridsSortedByAnchor) {
  // Two separate grids that do not touch:
  // Top grid at y = [0, 60], x = [0, 60].
  // Bottom grid at y = [100, 160], x = [0, 60].
  std::vector<RulingLine> ruling_lines = {
      CreateHLine(0, 0, 60),   CreateHLine(30, 0, 60),
      CreateHLine(60, 0, 60),  CreateHLine(100, 0, 60),
      CreateHLine(130, 0, 60), CreateHLine(160, 0, 60),
      CreateVLine(0, 0, 60),   CreateVLine(0, 100, 160),
      CreateVLine(30, 0, 60),  CreateVLine(30, 100, 160),
      CreateVLine(60, 0, 60),  CreateVLine(60, 100, 160),
  };

  // Text runs are ordered such that the lower grid's runs come first.
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs =
      CreateRunsInAllCells(kTwoCellBoundaries, {100, 130, 160});
  base::Extend(text_runs,
               CreateRunsInAllCells(kTwoCellBoundaries, kTwoCellBoundaries));

  std::vector<HeuristicTable> tables =
      DetectGridTables(text_runs, ruling_lines);
  ASSERT_EQ(tables.size(), 2u);

  EXPECT_EQ(tables[0].anchor_text_run_index, 0u);
  EXPECT_EQ(tables[0].bounds, gfx::RectF(0, 100, 60, 60));

  EXPECT_EQ(tables[1].anchor_text_run_index, 4u);
  EXPECT_EQ(tables[1].bounds, gfx::RectF(0, 0, 60, 60));
}

TEST(PdfAccessibilityTableDetectorTest, NestedGridWithSameAnchorKeepsOuter) {
  // The nested grid's runs come first, so both grids have anchor 0.
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs =
      CreateRunsInAllCells(kNestedGridXs, kNestedGridYs);
  base::Extend(text_runs,
               CreateRunsInCells(kOuterGridXs, kOuterGridYs,
                                 {{0, 1}, {0, 2}, {1, 0}, {1, 1}, {1, 2}}));

  std::vector<HeuristicTable> tables =
      DetectGridTables(text_runs, CreateNestedGridLines());
  ASSERT_EQ(tables.size(), 1u);
  EXPECT_EQ(tables[0].bounds, gfx::RectF(0, 0, 360, 240));
  EXPECT_EQ(tables[0].anchor_text_run_index, 0u);
  ASSERT_EQ(tables[0].rows.size(), 2u);
  ASSERT_EQ(tables[0].rows[0].cells.size(), 3u);

  // The top-left cell holds the nested grid's 20 runs, in ascending order.
  const std::vector<size_t>& indices =
      tables[0].rows[0].cells[0].text_run_indices;
  ASSERT_EQ(indices.size(), 20u);
  EXPECT_EQ(indices.front(), 0u);
  EXPECT_EQ(indices.back(), 19u);
}

TEST(PdfAccessibilityTableDetectorTest, NestedGridWithLaterAnchorKeepsOuter) {
  // The outer grid's top-middle run comes first, so the outer grid has anchor 0
  // and the nested grid has anchor 1.
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs =
      CreateRunsInCells(kOuterGridXs, kOuterGridYs, {{0, 1}});
  base::Extend(text_runs, CreateRunsInAllCells(kNestedGridXs, kNestedGridYs));
  base::Extend(text_runs, CreateRunsInCells(kOuterGridXs, kOuterGridYs,
                                            {{0, 2}, {1, 0}, {1, 1}, {1, 2}}));

  std::vector<HeuristicTable> tables =
      DetectGridTables(text_runs, CreateNestedGridLines());
  ASSERT_EQ(tables.size(), 1u);
  EXPECT_EQ(tables[0].bounds, gfx::RectF(0, 0, 360, 240));
  EXPECT_EQ(tables[0].anchor_text_run_index, 0u);
  ASSERT_EQ(tables[0].rows.size(), 2u);
  ASSERT_EQ(tables[0].rows[0].cells.size(), 3u);

  // The top-left cell holds the nested grid's 20 runs, in ascending order.
  const std::vector<size_t>& indices =
      tables[0].rows[0].cells[0].text_run_indices;
  ASSERT_EQ(indices.size(), 20u);
  EXPECT_EQ(indices.front(), 1u);
  EXPECT_EQ(indices.back(), 20u);
}

TEST(PdfAccessibilityTableDetectorTest,
     NestedGridInOtherwiseEmptyGridKeepsNested) {
  // Only the outer grid's top-left cell has text, so the outer grid is pruned
  // to one cell and skipped before overlaps are checked.
  std::vector<HeuristicTable> tables =
      DetectGridTables(CreateRunsInAllCells(kNestedGridXs, kNestedGridYs),
                       CreateNestedGridLines());
  ASSERT_EQ(tables.size(), 1u);
  EXPECT_EQ(tables[0].bounds, gfx::RectF(10, 10, 100, 100));
  ASSERT_EQ(tables[0].rows.size(), 4u);
  EXPECT_EQ(tables[0].rows[0].cells.size(), 5u);
}

TEST(PdfAccessibilityTableDetectorTest, ExcludedRunInOuterGridKeepsNestedGrid) {
  // Same layout as `NestedGridWithLaterAnchorKeepsOuter`, but excluding the
  // outer grid's top-middle run rejects the outer grid before overlapping
  // tables are removed, so the nested grid is kept.
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs =
      CreateRunsInCells(kOuterGridXs, kOuterGridYs, {{0, 1}});
  base::Extend(text_runs, CreateRunsInAllCells(kNestedGridXs, kNestedGridYs));
  base::Extend(text_runs, CreateRunsInCells(kOuterGridXs, kOuterGridYs,
                                            {{0, 2}, {1, 0}, {1, 1}, {1, 2}}));

  std::vector<HeuristicTable> tables =
      DetectGridTables(text_runs, CreateNestedGridLines(),
                       /*excluded_run_indices=*/{0});
  ASSERT_EQ(tables.size(), 1u);
  EXPECT_EQ(tables[0].bounds, gfx::RectF(10, 10, 100, 100));
  EXPECT_EQ(tables[0].anchor_text_run_index, 1u);
}

TEST(PdfAccessibilityTableDetectorTest, RejectsGridWithRightToLeftRun) {
  std::vector<RulingLine> ruling_lines =
      CreateGridLines(kTwoCellBoundaries, kTwoCellBoundaries);
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs =
      CreateRunsInCells(kTwoCellBoundaries, kTwoCellBoundaries,
                        {{0, 0}, {0, 1}, {1, 0}});
  base::Extend(
      text_runs,
      CreateRunsInCells(kTwoCellBoundaries, kTwoCellBoundaries, {{1, 1}},
                        chrome_pdf::AccessibilityTextDirection::kRightToLeft));

  EXPECT_TRUE(DetectGridTables(text_runs, ruling_lines).empty());
}

TEST(PdfAccessibilityTableDetectorTest,
     AcceptsGridWithRightToLeftRunOutsideGrid) {
  std::vector<RulingLine> ruling_lines =
      CreateGridLines(kTwoCellBoundaries, kTwoCellBoundaries);
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs =
      CreateRunsInCells(kTwoCellBoundaries, kTwoCellBoundaries,
                        {{0, 0}, {0, 1}, {1, 0}, {1, 1}});
  // A right-to-left run below the grid.
  text_runs.push_back(
      CreateTextRun(gfx::RectF(5, 100, 20, 20),
                    chrome_pdf::AccessibilityTextDirection::kRightToLeft));

  EXPECT_EQ(DetectGridTables(text_runs, ruling_lines).size(), 1u);
}

TEST(PdfAccessibilityTableDetectorTest, GetTableTextRunIndicesEmpty) {
  EXPECT_THAT(GetTableTextRunIndices(HeuristicTable()), testing::IsEmpty());
}

TEST(PdfAccessibilityTableDetectorTest, GetTableTextRunIndicesRowByRow) {
  // Indices go row by row, left to right, and skip empty cells.
  //
  // With 2 rows and 3 columns, going column by column would give a different
  // order: 4, 1, 2, 5, 0, 3.
  HeuristicTable table;
  table.rows = {
      {.cells = {{.text_run_indices = {4}}, {}, {.text_run_indices = {0}}}},
      {.cells = {{.text_run_indices = {1, 2}},
                 {.text_run_indices = {5}},
                 {.text_run_indices = {3}}}},
  };
  EXPECT_THAT(GetTableTextRunIndices(table),
              testing::ElementsAre(4u, 0u, 1u, 2u, 5u, 3u));
}

}  // namespace pdf
