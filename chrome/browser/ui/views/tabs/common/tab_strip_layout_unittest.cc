// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <numeric>
#include <vector>

#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_layout_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

int GetStandardTabWidth() {
  return TabStyle::Get()->GetStandardWidth(/*is_split=*/false);
}

int GetMinTabWidth() {
  return TabStyle::Get()->GetMinimumInactiveWidth();
}

}  // namespace

TEST(TabStripLayoutUtilsTest, ProportionalWidths_FitPreferred) {
  const int standard_width = GetStandardTabWidth();
  const int min_width = GetMinTabWidth();
  std::vector<int> preferred = {standard_width, standard_width, standard_width};
  std::vector<int> min = {min_width, min_width, min_width};
  int total_pref = 3 * standard_width;
  int total_min = 3 * min_width;

  std::vector<int> result = CalculateProportionalChildWidths(
      total_pref + 100, preferred, min, total_pref, total_min);
  EXPECT_EQ(result, preferred);
}

TEST(TabStripLayoutUtilsTest, ProportionalWidths_AtTotalMin) {
  const int standard_width = GetStandardTabWidth();
  const int min_width = GetMinTabWidth();
  std::vector<int> preferred = {standard_width, standard_width, standard_width};
  std::vector<int> min = {min_width, min_width, min_width};
  int total_pref = 3 * standard_width;
  int total_min = 3 * min_width;

  std::vector<int> result = CalculateProportionalChildWidths(
      total_min - 50, preferred, min, total_pref, total_min);
  EXPECT_EQ(result, min);
}

TEST(TabStripLayoutUtilsTest, ProportionalWidths_Balanced1PassDistribution) {
  constexpr size_t kNumTabs = 10;
  const int standard_width = GetStandardTabWidth();
  const int min_width = GetMinTabWidth();
  std::vector<int> preferred(kNumTabs, standard_width);
  std::vector<int> min(kNumTabs, min_width);
  int total_pref = kNumTabs * standard_width;
  int total_min = kNumTabs * min_width;

  // Available width is 1073px. Ideal average width is 107.3px.
  constexpr int kAvailableWidth = 1073;
  std::vector<int> result = CalculateProportionalChildWidths(
      kAvailableWidth, preferred, min, total_pref, total_min);

  int sum = std::accumulate(result.begin(), result.end(), 0);
  EXPECT_EQ(sum, kAvailableWidth);

  // Every single tab must be either 107px or 108px.
  // No tab should be dumped with a large rounding error (e.g. 100px).
  for (int width : result) {
    EXPECT_GE(width, 107);
    EXPECT_LE(width, 108);
  }
}

TEST(TabStripLayoutUtilsTest, ProportionalWidths_TabClosingInvariance) {
  constexpr size_t kInitialTabs = 10;
  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  const int standard_width = GetStandardTabWidth();
  const int min_width = GetMinTabWidth();

  std::vector<int> preferred(kInitialTabs, standard_width);
  std::vector<int> min(kInitialTabs, min_width);
  int total_pref = kInitialTabs * standard_width;
  int total_min = kInitialTabs * min_width;

  int available_width = 1000;
  std::vector<int> initial_widths = CalculateProportionalChildWidths(
      available_width, preferred, min, total_pref, total_min);

  int base_width = initial_widths[0];

  // Simulate closing tab at index 3.
  int closed_tab_width = initial_widths[3];
  int new_available_width = available_width - closed_tab_width + tab_overlap;

  std::vector<int> preferred_after(kInitialTabs - 1, standard_width);
  std::vector<int> min_after(kInitialTabs - 1, min_width);
  int total_pref_after = (kInitialTabs - 1) * standard_width;
  int total_min_after = (kInitialTabs - 1) * min_width;

  std::vector<int> after_close_widths = CalculateProportionalChildWidths(
      new_available_width, preferred_after, min_after, total_pref_after,
      total_min_after);

  // Verify that all remaining tabs do NOT shrink below their base width.
  for (int width : after_close_widths) {
    EXPECT_GE(width, base_width);
  }
}

TEST(TabStripLayoutUtilsTest, ProportionalWidths_ConsecutiveClosesNoDrift) {
  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  const int standard_width = GetStandardTabWidth();
  const int min_width = GetMinTabWidth();
  int num_tabs = 12;
  int available_width = 900;

  std::vector<int> preferred(num_tabs, standard_width);
  std::vector<int> min(num_tabs, min_width);
  int total_pref = num_tabs * standard_width;
  int total_min = num_tabs * min_width;

  std::vector<int> current_widths = CalculateProportionalChildWidths(
      available_width, preferred, min, total_pref, total_min);
  int starting_base_width = current_widths[0];

  // Close tabs consecutively from 12 down to 4.
  while (num_tabs > 4) {
    int closing_width = current_widths[1];
    available_width = available_width - closing_width + tab_overlap;
    num_tabs--;

    std::vector<int> pref_step(num_tabs, standard_width);
    std::vector<int> min_step(num_tabs, min_width);
    current_widths = CalculateProportionalChildWidths(
        available_width, pref_step, min_step, num_tabs * standard_width,
        num_tabs * min_width);

    // Verify tabs never shrink below starting_base_width across consecutive
    // closes.
    for (int width : current_widths) {
      EXPECT_GE(width, starting_base_width);
    }
  }
}
