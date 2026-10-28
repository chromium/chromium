// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_table_detector.h"

#include <array>
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

}  // namespace pdf
