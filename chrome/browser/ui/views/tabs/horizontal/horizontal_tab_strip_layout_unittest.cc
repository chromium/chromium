// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <numeric>
#include <vector>

#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_layout_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

int GetPreferredTabWidth(bool is_split = false) {
  return TabStyle::Get()->GetStandardWidth(is_split);
}

int GetMinInactiveTabWidth() {
  return TabStyle::Get()->GetMinimumInactiveWidth();
}

int GetMinActiveTabWidth(bool is_split = false) {
  return TabStyle::Get()->GetMinimumActiveWidth(is_split);
}

}  // namespace

TEST(HorizontalTabStripLayoutTest, AvailableWidthMatchesTotalPreferred) {
  const int preferred_width = GetPreferredTabWidth();
  const int min_width = GetMinInactiveTabWidth();
  const std::vector<int> preferred = {preferred_width, preferred_width,
                                      preferred_width};
  const std::vector<int> min = {min_width, min_width, min_width};
  const int total_preferred = preferred_width * 3;
  const int total_min = min_width * 3;

  auto widths = CalculateProportionalChildWidths(
      total_preferred, preferred, min, total_preferred, total_min);
  EXPECT_EQ(widths, preferred);
}

TEST(HorizontalTabStripLayoutTest, AvailableWidthBelowTotalMinWidth) {
  const int preferred_width = GetPreferredTabWidth();
  const int min_width = GetMinInactiveTabWidth();
  const std::vector<int> preferred = {preferred_width, preferred_width,
                                      preferred_width};
  const std::vector<int> min = {min_width, min_width, min_width};
  const int total_preferred = preferred_width * 3;
  const int total_min = min_width * 3;

  auto widths = CalculateProportionalChildWidths(total_min - 20, preferred, min,
                                                 total_preferred, total_min);
  EXPECT_EQ(widths, min);
}

TEST(HorizontalTabStripLayoutTest, MiddleWidthRoundedDistributesRemainder) {
  const int preferred_width = GetPreferredTabWidth();
  const int min_width = GetMinInactiveTabWidth();
  // With 4 tabs in a 600px width container tabs must shrink and width is
  // distributed evenly. Tab widths should differ by at most 1px.
  const int num_tabs = 4;
  const std::vector<int> preferred(num_tabs, preferred_width);
  const std::vector<int> min(num_tabs, min_width);
  const int total_preferred = preferred_width * num_tabs;
  const int total_min = min_width * num_tabs;
  const int available_width = 600;

  auto widths = CalculateProportionalChildWidths(
      available_width, preferred, min, total_preferred, total_min);

  int sum = std::accumulate(widths.begin(), widths.end(), 0);
  EXPECT_EQ(sum, available_width);

  // All tabs must be within 1px of each other.
  int min_allocated = *std::min_element(widths.begin(), widths.end());
  int max_allocated = *std::max_element(widths.begin(), widths.end());
  EXPECT_LE(max_allocated - min_allocated, 1);
}

TEST(HorizontalTabStripLayoutTest, TabsWithGroupHeaderProportionalShrinking) {
  const int preferred_width = GetPreferredTabWidth();
  const int min_width = GetMinInactiveTabWidth();
  // A tab group header has a fixed preferred/minimum size and does not shrink.
  const int header_width = 50;
  const std::vector<int> preferred = {header_width, preferred_width,
                                      preferred_width, preferred_width};
  const std::vector<int> min = {header_width, min_width, min_width, min_width};
  const int total_preferred = header_width + preferred_width * 3;
  const int total_min = header_width + min_width * 3;
  const int available_width = 450;

  auto widths = CalculateProportionalChildWidths(
      available_width, preferred, min, total_preferred, total_min);

  int sum = std::accumulate(widths.begin(), widths.end(), 0);
  EXPECT_EQ(sum, available_width);

  // Header preserves its exact size.
  EXPECT_EQ(widths[0], header_width);

  // The 3 normal tabs proportionally share the remaining space evenly
  // (at most 1px delta among them).
  int min_tab = *std::min_element(widths.begin() + 1, widths.end());
  int max_tab = *std::max_element(widths.begin() + 1, widths.end());
  EXPECT_LE(max_tab - min_tab, 1);
}

TEST(HorizontalTabStripLayoutTest,
     BelowMinActiveWidthInactiveTabsShrinkFurther) {
  const int preferred_width = GetPreferredTabWidth();
  const int active_min_width = GetMinActiveTabWidth();
  const int inactive_min_width = GetMinInactiveTabWidth();

  // 1 active tab and 4 inactive tabs.
  const std::vector<int> preferred = {preferred_width, preferred_width,
                                      preferred_width, preferred_width,
                                      preferred_width};
  const std::vector<int> min = {inactive_min_width, inactive_min_width,
                                active_min_width, inactive_min_width,
                                inactive_min_width};
  const int total_preferred = preferred_width * 5;
  const int total_min = inactive_min_width * 4 + active_min_width;

  // Constrain space below what standard active minimums would allow for all
  // tabs.
  const int available_width = 250;
  auto widths = CalculateProportionalChildWidths(
      available_width, preferred, min, total_preferred, total_min);

  int sum = std::accumulate(widths.begin(), widths.end(), 0);
  EXPECT_EQ(sum, available_width);

  // Verify active tab remains larger than the inactive tabs to preserve its
  // close button.
  EXPECT_GE(widths[2], active_min_width);
  for (size_t i : {0, 1, 3, 4}) {
    EXPECT_GT(widths[2], widths[i]);
  }
}

TEST(HorizontalTabStripLayoutTest,
     SplitTabsAndStandardTabsProportionalShrinking) {
  const int standard_preferred = GetPreferredTabWidth(/*is_split=*/false);
  const int split_preferred = GetPreferredTabWidth(/*is_split=*/true);
  const int min_width = GetMinInactiveTabWidth();

  // 2 split tabs and 2 standard tabs.
  const std::vector<int> preferred = {split_preferred, split_preferred,
                                      standard_preferred, standard_preferred};
  const std::vector<int> min = {min_width, min_width, min_width, min_width};
  const int total_preferred = split_preferred * 2 + standard_preferred * 2;
  const int total_min = min_width * 4;

  const int available_width = 500;

  auto widths = CalculateProportionalChildWidths(
      available_width, preferred, min, total_preferred, total_min);

  int sum = std::accumulate(widths.begin(), widths.end(), 0);
  EXPECT_EQ(sum, available_width);

  // Split tabs with smaller preferred width should end up smaller than standard
  // tabs.
  EXPECT_LT(widths[0], widths[2]);
  EXPECT_LT(widths[1], widths[3]);
  EXPECT_LE(std::abs(widths[0] - widths[1]), 1);
  EXPECT_LE(std::abs(widths[2] - widths[3]), 1);
}
