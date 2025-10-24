// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_layout_params.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"

namespace {

// This is the starting content area.
constexpr gfx::Rect kContentArea{11, 12, 100, 120};

// These are the starting exclusions.
constexpr BrowserLayoutExclusionArea kLeadingExclusion{
    .content{20.f, 15.f},
    .horizontal_padding = 30.f,
    .vertical_padding = 25.f};
constexpr BrowserLayoutExclusionArea kTrailingExclusion{
    .content{21.f, 16.f},
    .horizontal_padding = 31.f,
    .vertical_padding = 26.f};

consteval gfx::Rect Inset(const gfx::Rect rect, const gfx::Insets insets) {
  return gfx::Rect(rect.x() + insets.left(), rect.y() + insets.top(),
                   rect.width() - insets.width(),
                   rect.height() - insets.height());
}
consteval gfx::Insets Add(const gfx::Insets first, const gfx::Insets second) {
  return gfx::Insets::TLBR(
      first.top() + second.top(), first.left() + second.left(),
      first.bottom() + second.bottom(), first.right() + second.right());
}

// Slightly inside the original area.
constexpr gfx::Insets kSmallInsets = gfx::Insets::TLBR(4, 2, 0, 3);
constexpr gfx::Rect kSmallerAreaRect = Inset(kContentArea, kSmallInsets);

// Aligned with the corners of the exclusion areas, with top aligned with the
// leading edge.
constexpr gfx::Insets kAlignedInsets = gfx::Insets::TLBR(15, 20, 0, 21);
constexpr gfx::Rect kAlignedAreaRect = Inset(kContentArea, kAlignedInsets);

// Eats into the margins of both exclusion areas.
constexpr gfx::Insets kMarginAdjustments = gfx::Insets::TLBR(3, 4, 0, 5);
constexpr gfx::Insets kMarginInsets = Add(kAlignedInsets, kMarginAdjustments);
constexpr gfx::Rect kIntoMarginsRect = Inset(kContentArea, kMarginInsets);

}  // namespace

TEST(BrowserViewLayoutParamsTest,
     BrowserLayoutParams_InLocalCoordinates_Same_NoClientInsets) {
  static constexpr BrowserLayoutParams kParams{
      .visual_client_area = gfx::Rect{{}, kContentArea.size()},
      .leading_exclusion = kLeadingExclusion,
      .trailing_exclusion = kTrailingExclusion};
  const BrowserLayoutParams result =
      kParams.InLocalCoordinates(gfx::Rect({}, kContentArea.size()));
  EXPECT_EQ(kParams, result);
}

TEST(BrowserViewLayoutParamsTest,
     BrowserLayoutParams_InLocalCoordinates_Same_WithClientInsets) {
  static constexpr BrowserLayoutParams kParams{
      .visual_client_area = kContentArea,
      .leading_exclusion = kLeadingExclusion,
      .trailing_exclusion = kTrailingExclusion};
  const BrowserLayoutParams result = kParams.InLocalCoordinates(kContentArea);
  static constexpr BrowserLayoutParams kExpected{
      .visual_client_area{{}, kContentArea.size()},
      .leading_exclusion = kLeadingExclusion,
      .trailing_exclusion = kTrailingExclusion};
  EXPECT_EQ(kExpected, result);
}

TEST(BrowserViewLayoutParamsTest,
     BrowserLayoutParams_InLocalCoordinates_Different_OverlapsContent) {
  static constexpr BrowserLayoutParams kParams{
      .visual_client_area = kContentArea,
      .leading_exclusion = kLeadingExclusion,
      .trailing_exclusion = kTrailingExclusion};
  const BrowserLayoutParams result =
      kParams.InLocalCoordinates(kSmallerAreaRect);
  static constexpr BrowserLayoutParams kExpected{
      .visual_client_area{{}, kSmallerAreaRect.size()},
      .leading_exclusion{
          .content{kLeadingExclusion.content.width() - kSmallInsets.left(),
                   kLeadingExclusion.content.height() - kSmallInsets.top()},
          .horizontal_padding = kLeadingExclusion.horizontal_padding,
          .vertical_padding = kLeadingExclusion.vertical_padding},
      .trailing_exclusion{
          .content{kTrailingExclusion.content.width() - kSmallInsets.right(),
                   kTrailingExclusion.content.height() - kSmallInsets.top()},
          .horizontal_padding = kTrailingExclusion.horizontal_padding,
          .vertical_padding = kTrailingExclusion.vertical_padding}};
  EXPECT_EQ(kExpected, result);
}

TEST(BrowserViewLayoutParamsTest,
     BrowserLayoutParams_InLocalCoordinates_Different_AlignsWithContent) {
  static constexpr BrowserLayoutParams kParams{
      .visual_client_area = kContentArea,
      .leading_exclusion = kLeadingExclusion,
      .trailing_exclusion = kTrailingExclusion};
  const BrowserLayoutParams result =
      kParams.InLocalCoordinates(kAlignedAreaRect);
  static constexpr BrowserLayoutParams kExpected{
      .visual_client_area{{}, kAlignedAreaRect.size()},
      .leading_exclusion{
          .horizontal_padding = kLeadingExclusion.horizontal_padding,
          .vertical_padding = kLeadingExclusion.vertical_padding},
      .trailing_exclusion{
          // The content area of the trailing exclusion is 1px taller, so 1px
          // remains.
          .content{0.f, 1.f},
          .horizontal_padding = kTrailingExclusion.horizontal_padding,
          .vertical_padding = kTrailingExclusion.vertical_padding}};
  EXPECT_EQ(kExpected, result);
}

TEST(BrowserViewLayoutParamsTest,
     BrowserLayoutParams_InLocalCoordinates_Different_CutsIntoMargins) {
  static constexpr BrowserLayoutParams kParams{
      .visual_client_area = kContentArea,
      .leading_exclusion = kLeadingExclusion,
      .trailing_exclusion = kTrailingExclusion};
  const BrowserLayoutParams result =
      kParams.InLocalCoordinates(kIntoMarginsRect);
  static constexpr BrowserLayoutParams kExpected{
      .visual_client_area{{}, kIntoMarginsRect.size()},
      .leading_exclusion{
          .horizontal_padding =
              kLeadingExclusion.horizontal_padding - kMarginAdjustments.left(),
          .vertical_padding =
              kLeadingExclusion.vertical_padding - kMarginAdjustments.top()},
      .trailing_exclusion{
          .horizontal_padding = kTrailingExclusion.horizontal_padding -
                                kMarginAdjustments.right(),
          // Note the difference of 1 here because the trailing content area is
          // larger by one pixel.
          .vertical_padding = kTrailingExclusion.vertical_padding -
                              (kMarginAdjustments.top() - 1)}};
  EXPECT_EQ(kExpected, result);
}
