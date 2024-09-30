// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(OmniboxMatchCellViewTest, ComputeMatchMaxWidths) {
  int contents_max_width, description_max_width, iph_link_max_width;
  const int separator_width = 10;
  const int kMinimumContentsWidth = 300;
  int contents_width, description_width, iph_link_view_width, available_width;

  // Contents, description, and IPH link fit fine.
  contents_width = 100;
  description_width = 50;
  iph_link_view_width = 30;
  available_width = 200;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, iph_link_view_width,
      available_width, true, &contents_max_width, &description_max_width,
      &iph_link_max_width);
  EXPECT_EQ(contents_max_width, contents_width);
  EXPECT_EQ(description_max_width, description_width);
  EXPECT_EQ(iph_link_max_width, 30);

  // IPH link should be given priority.
  contents_width = 100;
  description_width = 50;
  iph_link_view_width = 30;
  available_width = 20;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, iph_link_view_width,
      available_width, true, &contents_max_width, &description_max_width,
      &iph_link_max_width);
  EXPECT_EQ(contents_max_width, 0);
  EXPECT_EQ(description_max_width, 0);
  EXPECT_EQ(iph_link_max_width, 20);

  // Contents should be given as much space as it wants up to 300 pixels.
  contents_width = 100;
  description_width = 50;
  iph_link_view_width = 30;
  available_width = 130;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, iph_link_view_width,
      available_width, true, &contents_max_width, &description_max_width,
      &iph_link_max_width);
  EXPECT_EQ(contents_max_width, contents_width);
  EXPECT_EQ(description_max_width, 0);
  EXPECT_EQ(iph_link_max_width, 30);

  // Both contents and description will be limited.
  contents_width = 310;
  description_width = 150;
  iph_link_view_width = 30;
  available_width = 430;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, iph_link_view_width,
      available_width, true, &contents_max_width, &description_max_width,
      &iph_link_max_width);
  EXPECT_EQ(contents_max_width, kMinimumContentsWidth);
  EXPECT_EQ(description_max_width, available_width - kMinimumContentsWidth -
                                       separator_width - iph_link_max_width);
  EXPECT_EQ(iph_link_max_width, 30);

  // Contents takes all available space not taken by IPH link.
  contents_width = 400;
  description_width = 0;
  iph_link_view_width = 30;
  available_width = 230;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, iph_link_view_width,
      available_width, true, &contents_max_width, &description_max_width,
      &iph_link_max_width);
  EXPECT_EQ(contents_max_width, available_width - iph_link_max_width);
  EXPECT_EQ(description_max_width, 0);
  EXPECT_EQ(iph_link_max_width, 30);

  // Half and half.
  contents_width = 395;
  description_width = 395;
  iph_link_view_width = 0;
  available_width = 700;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, iph_link_view_width,
      available_width, true, &contents_max_width, &description_max_width,
      &iph_link_max_width);
  EXPECT_EQ(contents_max_width, 345);
  EXPECT_EQ(description_max_width, 345);
  EXPECT_EQ(iph_link_max_width, 0);

  // When we disallow shrinking the contents, it should get as much space as
  // it wants.
  contents_width = 395;
  description_width = 395;
  iph_link_view_width = 0;
  available_width = 700;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, iph_link_view_width,
      available_width, false, &contents_max_width, &description_max_width,
      &iph_link_max_width);
  EXPECT_EQ(contents_max_width, contents_width);
  EXPECT_EQ(description_max_width, available_width - contents_width -
                                       separator_width - iph_link_max_width);
  EXPECT_EQ(iph_link_max_width, 0);

  // (available_width - separator_width) is odd, so contents gets the extra
  // pixel.
  contents_width = 395;
  description_width = 395;
  iph_link_view_width = 0;
  available_width = 699;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, iph_link_view_width,
      available_width, true, &contents_max_width, &description_max_width,
      &iph_link_max_width);
  EXPECT_EQ(contents_max_width, 345);
  EXPECT_EQ(description_max_width, 344);
  EXPECT_EQ(iph_link_max_width, 0);

  // Not enough space to draw anything.
  contents_width = 1;
  description_width = 1;
  iph_link_view_width = 0;
  available_width = 0;
  OmniboxMatchCellView::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, iph_link_view_width,
      available_width, true, &contents_max_width, &description_max_width,
      &iph_link_max_width);
  EXPECT_EQ(contents_max_width, 0);
  EXPECT_EQ(description_max_width, 0);
  EXPECT_EQ(iph_link_max_width, 0);
}
