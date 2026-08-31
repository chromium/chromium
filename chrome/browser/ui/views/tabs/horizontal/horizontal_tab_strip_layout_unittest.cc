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

TEST(HorizontalTabStripLayoutTest, AllTabsHaveEqualWidthsAboveActiveMinWidth) {
  const int preferred_width = GetPreferredTabWidth();
  const int active_min_width = GetMinActiveTabWidth();
  const int inactive_min_width = GetMinInactiveTabWidth();
  const int crossover_width = active_min_width;

  const int num_tabs = 4;
  const std::vector<int> preferred(num_tabs, preferred_width);
  const std::vector<int> crossover(num_tabs, crossover_width);

  const int total_preferred = preferred_width * num_tabs;
  const int total_crossover = crossover_width * num_tabs;

  const int available_width = 600;

  // Test that activating each tab index (0 through 3) results in the exact same
  // uniform tab width distribution.
  std::vector<int> reference_widths;
  for (int active_index = 0; active_index < num_tabs; ++active_index) {
    std::vector<int> min(num_tabs, inactive_min_width);
    min[active_index] = active_min_width;
    const int total_min =
        inactive_min_width * (num_tabs - 1) + active_min_width;

    auto widths = CalculateProportionalChildWidths(
        available_width, preferred, crossover, min, total_preferred,
        total_crossover, total_min);

    int sum = std::accumulate(widths.begin(), widths.end(), 0);
    EXPECT_EQ(sum, available_width);

    // When preferred tab size is above min active tab size, active and inactive
    // tabs must have equal widths (within 1px remainder rounding delta).
    int min_allocated = *std::min_element(widths.begin(), widths.end());
    int max_allocated = *std::max_element(widths.begin(), widths.end());
    EXPECT_LE(max_allocated - min_allocated, 1);

    if (reference_widths.empty()) {
      reference_widths = widths;
    } else {
      // Verify switching active tab doesn't alter the allocated widths.
      EXPECT_EQ(widths, reference_widths);
    }
  }
}

TEST(HorizontalTabStripLayoutTest,
     ActiveTabHoldsAtMinimumActiveWidthBelowCrossover) {
  const int preferred_width = GetPreferredTabWidth();
  const int active_min_width = GetMinActiveTabWidth();
  const int inactive_min_width = GetMinInactiveTabWidth();
  const int crossover_width = active_min_width;

  const int num_tabs = 4;
  const std::vector<int> preferred(num_tabs, preferred_width);
  const std::vector<int> crossover(num_tabs, crossover_width);

  // Tab 0 is active, tabs 1-3 are inactive.
  const std::vector<int> min = {active_min_width, inactive_min_width,
                                inactive_min_width, inactive_min_width};

  const int total_preferred = preferred_width * num_tabs;
  const int total_crossover = crossover_width * num_tabs;
  const int total_min = active_min_width + inactive_min_width * 3;

  // Available width is below total crossover (4 * 68 = 272px) but above total
  // min.
  const int available_width = 200;
  auto widths = CalculateProportionalChildWidths(
      available_width, preferred, crossover, min, total_preferred,
      total_crossover, total_min);

  int sum = std::accumulate(widths.begin(), widths.end(), 0);
  EXPECT_EQ(sum, available_width);

  // Active tab must stay at least active_min_width to preserve its close
  // button.
  EXPECT_GE(widths[0], active_min_width);

  // Inactive tabs must shrink below active_min_width and share remaining space.
  for (size_t i = 1; i < num_tabs; ++i) {
    EXPECT_LT(widths[i], widths[0]);
    EXPECT_GE(widths[i], inactive_min_width);
  }
}

TEST(HorizontalTabStripLayoutTest,
     GroupAndTabsProportionalSizingWithCrossover) {
  const int preferred_tab_width = GetPreferredTabWidth();
  const int active_min_width = GetMinActiveTabWidth();
  const int inactive_min_width = GetMinInactiveTabWidth();
  const int crossover_tab_width = active_min_width;
  const int header_width = 50;

  // 1 group with 2 tabs and 2 ungrouped tabs.
  const int group_pref = header_width + preferred_tab_width * 2;
  const int group_crossover = header_width + crossover_tab_width * 2;
  const int group_min = header_width + inactive_min_width * 2;

  const std::vector<int> preferred = {group_pref, preferred_tab_width,
                                      preferred_tab_width};
  const std::vector<int> crossover = {group_crossover, crossover_tab_width,
                                      crossover_tab_width};
  const std::vector<int> min = {group_min, inactive_min_width,
                                inactive_min_width};

  const int total_preferred = group_pref + preferred_tab_width * 2;
  const int total_crossover = group_crossover + crossover_tab_width * 2;
  const int total_min = group_min + inactive_min_width * 2;

  const int available_width = 600;
  auto parent_widths = CalculateProportionalChildWidths(
      available_width, preferred, crossover, min, total_preferred,
      total_crossover, total_min);

  int sum = std::accumulate(parent_widths.begin(), parent_widths.end(), 0);
  EXPECT_EQ(sum, available_width);

  // Now distribute the group's allocated width to its child tabs.
  const int group_allocated = parent_widths[0];
  const int space_for_group_tabs = group_allocated - header_width;

  const std::vector<int> group_child_pref = {preferred_tab_width,
                                             preferred_tab_width};
  const std::vector<int> group_child_crossover = {crossover_tab_width,
                                                  crossover_tab_width};
  const std::vector<int> group_child_min = {inactive_min_width,
                                            inactive_min_width};

  auto group_tab_widths = CalculateProportionalChildWidths(
      space_for_group_tabs, group_child_pref, group_child_crossover,
      group_child_min, preferred_tab_width * 2, crossover_tab_width * 2,
      inactive_min_width * 2);

  // Verify tabs inside group match the width of ungrouped tabs (within 1px).
  EXPECT_LE(std::abs(group_tab_widths[0] - parent_widths[1]), 1);
  EXPECT_LE(std::abs(group_tab_widths[1] - parent_widths[2]), 1);
}

TEST(HorizontalTabStripLayoutTest,
     CrowdedStripTabActivationMaintainsMinimumInactiveWidths) {
  const int preferred_width = GetPreferredTabWidth();
  const int active_min_width = GetMinActiveTabWidth();
  const int inactive_min_width = GetMinInactiveTabWidth();
  const int crossover_width = active_min_width;

  const int num_tabs = 20;
  const std::vector<int> preferred(num_tabs, preferred_width);
  const std::vector<int> crossover(num_tabs, crossover_width);

  const int total_preferred = preferred_width * num_tabs;
  const int total_crossover = crossover_width * num_tabs;

  // Available width is small (below total minimum width so strip is
  // scrolling/crowded).
  const int available_width = 400;

  // Test across all possible active tab indices (0 through 19).
  for (int active_index = 0; active_index < num_tabs; ++active_index) {
    std::vector<int> min(num_tabs, inactive_min_width);
    min[active_index] = active_min_width;
    const int total_min =
        inactive_min_width * (num_tabs - 1) + active_min_width;

    auto widths = CalculateProportionalChildWidths(
        available_width, preferred, crossover, min, total_preferred,
        total_crossover, total_min);

    // Active tab must be at minimum active width.
    EXPECT_EQ(widths[active_index], active_min_width);

    // Inactive tabs must remain at minimum inactive width.
    for (int i = 0; i < num_tabs; ++i) {
      if (i == active_index) {
        continue;
      }
      EXPECT_EQ(widths[i], inactive_min_width);
    }
  }
}

TEST(HorizontalTabStripLayoutTest,
     UnconstrainedPreferredWidthReflectsGroupExpansionState) {
  const int preferred_tab_width = GetPreferredTabWidth();
  const int header_width = 50;
  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  const int header_overlap = 2;

  // Calculate expected sizes for 2 ungrouped tabs and 1 collapsed group.
  const int collapsed_group_pref = header_width;
  const int collapsed_overlap_total = tab_overlap + header_overlap;
  const int collapsed_total_pref =
      2 * preferred_tab_width + collapsed_group_pref - collapsed_overlap_total;

  // Calculate expected sizes for 2 ungrouped tabs and 1 expanded group.
  const int group_internal_overlap = header_overlap + tab_overlap;
  const int expanded_group_pref =
      header_width + 2 * preferred_tab_width - group_internal_overlap;
  const int expanded_overlap_total = tab_overlap + header_overlap;
  const int expanded_total_pref =
      2 * preferred_tab_width + expanded_group_pref - expanded_overlap_total;

  // Verify expected sizes.
  EXPECT_GT(expanded_total_pref, collapsed_total_pref);
  EXPECT_EQ(expanded_total_pref - collapsed_total_pref,
            2 * preferred_tab_width - group_internal_overlap);
}
