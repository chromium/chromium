// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(OmniboxMatchCellViewTest, ComputeMatchMaxWidths) {
  int contents_max_width, description_max_width;
  const int separator_width = 10;
  const int kMinimumContentsWidth = 300;
  int contents_width, description_width, available_width;

  // Both contents and description fit fine.
  contents_width = 100;
  description_width = 50;
  available_width = 200;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(contents_width, contents_max_width);
  EXPECT_EQ(description_width, description_max_width);

  // Contents should be given as much space as it wants up to 300 pixels.
  contents_width = 100;
  description_width = 50;
  available_width = 100;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(contents_width, contents_max_width);
  EXPECT_EQ(0, description_max_width);

  // Description should be hidden if it's at least 75 pixels wide but doesn't
  // get 75 pixels of space.
  contents_width = 300;
  description_width = 100;
  available_width = 384;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(contents_width, contents_max_width);
  EXPECT_EQ(0, description_max_width);

  // If contents and description are on separate lines, each can take the full
  // available width.
  contents_width = 300;
  description_width = 100;
  available_width = 384;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width, true,
      true, &contents_max_width, &description_max_width);
  EXPECT_EQ(contents_width, contents_max_width);
  EXPECT_EQ(description_width, description_max_width);

  // Both contents and description will be limited.
  contents_width = 310;
  description_width = 150;
  available_width = 400;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(kMinimumContentsWidth, contents_max_width);
  EXPECT_EQ(available_width - kMinimumContentsWidth - separator_width,
            description_max_width);

  // Contents takes all available space.
  contents_width = 400;
  description_width = 0;
  available_width = 200;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(available_width, contents_max_width);
  EXPECT_EQ(0, description_max_width);

  // Large contents will be truncated but small description won't if two line
  // suggestion.
  contents_width = 400;
  description_width = 100;
  available_width = 200;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width, true,
      true, &contents_max_width, &description_max_width);
  EXPECT_EQ(available_width, contents_max_width);
  EXPECT_EQ(description_width, description_max_width);

  // Large description will be truncated but small contents won't if two line
  // suggestion.
  contents_width = 100;
  description_width = 400;
  available_width = 200;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width, true,
      true, &contents_max_width, &description_max_width);
  EXPECT_EQ(contents_width, contents_max_width);
  EXPECT_EQ(available_width, description_max_width);

  // Half and half.
  contents_width = 395;
  description_width = 395;
  available_width = 700;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(345, contents_max_width);
  EXPECT_EQ(345, description_max_width);

  // When we disallow shrinking the contents, it should get as much space as
  // it wants.
  contents_width = 395;
  description_width = 395;
  available_width = 700;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, false, &contents_max_width, &description_max_width);
  EXPECT_EQ(contents_width, contents_max_width);
  EXPECT_EQ((available_width - contents_width - separator_width),
            description_max_width);

  // (available_width - separator_width) is odd, so contents gets the extra
  // pixel.
  contents_width = 395;
  description_width = 395;
  available_width = 699;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(345, contents_max_width);
  EXPECT_EQ(344, description_max_width);

  // Not enough space to draw anything.
  contents_width = 1;
  description_width = 1;
  available_width = 0;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(0, contents_max_width);
  EXPECT_EQ(0, description_max_width);
}
