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
constexpr gfx::Insets kIntoMarginsInsets =
    Add(kAlignedInsets, kMarginAdjustments);
constexpr gfx::Rect kIntoMarginsRect = Inset(kContentArea, kIntoMarginsInsets);

}  // namespace

TEST(BrowserViewLayoutParamsTest, BrowserLayoutParams_Same_NoClientInsets) {
  static constexpr BrowserLayoutParams kParams{
      .visual_client_area = gfx::Rect{{}, kContentArea.size()},
      .leading_exclusion = kLeadingExclusion,
      .trailing_exclusion = kTrailingExclusion};
  static constexpr gfx::Rect kRect{{}, kContentArea.size()};
  const BrowserLayoutParams in_local_result = kParams.InLocalCoordinates(kRect);
  EXPECT_EQ(kParams, in_local_result);
  const BrowserLayoutParams with_client_area_result =
      kParams.WithClientArea(kRect);
  EXPECT_EQ(kParams, with_client_area_result);
  const BrowserLayoutParams with_insets_result =
      kParams.WithInsets(gfx::Insets());
  EXPECT_EQ(kParams, with_insets_result);
}

TEST(BrowserViewLayoutParamsTest, BrowserLayoutParams_Same_WithClientInsets) {
  static constexpr BrowserLayoutParams kParams{
      .visual_client_area = kContentArea,
      .leading_exclusion = kLeadingExclusion,
      .trailing_exclusion = kTrailingExclusion};
  BrowserLayoutParams expected{.visual_client_area{{}, kContentArea.size()},
                               .leading_exclusion = kLeadingExclusion,
                               .trailing_exclusion = kTrailingExclusion};
  const BrowserLayoutParams in_local_result =
      kParams.InLocalCoordinates(kContentArea);
  EXPECT_EQ(expected, in_local_result);
  expected.visual_client_area.set_origin(kContentArea.origin());
  const BrowserLayoutParams with_client_area_result =
      kParams.WithClientArea(kContentArea);
  EXPECT_EQ(expected, with_client_area_result);
  const BrowserLayoutParams with_insets_result =
      kParams.WithInsets(gfx::Insets());
  EXPECT_EQ(expected, with_insets_result);
}

TEST(BrowserViewLayoutParamsTest,
     BrowserLayoutParams_Different_OverlapsContent) {
  static constexpr BrowserLayoutParams kParams{
      .visual_client_area = kContentArea,
      .leading_exclusion = kLeadingExclusion,
      .trailing_exclusion = kTrailingExclusion};
  BrowserLayoutParams expected{
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
  const BrowserLayoutParams in_local_result =
      kParams.InLocalCoordinates(kSmallerAreaRect);
  EXPECT_EQ(expected, in_local_result);
  expected.visual_client_area.set_origin(kSmallerAreaRect.origin());
  const BrowserLayoutParams with_client_area_result =
      kParams.WithClientArea(kSmallerAreaRect);
  EXPECT_EQ(expected, with_client_area_result);
  const BrowserLayoutParams with_insets_result =
      kParams.WithInsets(kSmallInsets);
  EXPECT_EQ(expected, with_insets_result);
}

TEST(BrowserViewLayoutParamsTest,
     BrowserLayoutParams_InLocalCoordinates_Different_AlignsWithContent) {
  static constexpr BrowserLayoutParams kParams{
      .visual_client_area = kContentArea,
      .leading_exclusion = kLeadingExclusion,
      .trailing_exclusion = kTrailingExclusion};
  BrowserLayoutParams expected{
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
  const BrowserLayoutParams in_local_result =
      kParams.InLocalCoordinates(kAlignedAreaRect);
  EXPECT_EQ(expected, in_local_result);
  expected.visual_client_area.set_origin(kAlignedAreaRect.origin());
  const BrowserLayoutParams with_client_area_result =
      kParams.WithClientArea(kAlignedAreaRect);
  EXPECT_EQ(expected, with_client_area_result);
  const BrowserLayoutParams with_insets_result =
      kParams.WithInsets(kAlignedInsets);
  EXPECT_EQ(expected, with_insets_result);
}

TEST(BrowserViewLayoutParamsTest,
     BrowserLayoutParams_InLocalCoordinates_Different_CutsIntoMargins) {
  static constexpr BrowserLayoutParams kParams{
      .visual_client_area = kContentArea,
      .leading_exclusion = kLeadingExclusion,
      .trailing_exclusion = kTrailingExclusion};
  BrowserLayoutParams expected{
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
  const BrowserLayoutParams in_local_result =
      kParams.InLocalCoordinates(kIntoMarginsRect);
  EXPECT_EQ(expected, in_local_result);
  expected.visual_client_area.set_origin(kIntoMarginsRect.origin());
  const BrowserLayoutParams with_client_area_result =
      kParams.WithClientArea(kIntoMarginsRect);
  EXPECT_EQ(expected, with_client_area_result);
  const BrowserLayoutParams with_insets_result =
      kParams.WithInsets(kIntoMarginsInsets);
  EXPECT_EQ(expected, with_insets_result);
}
