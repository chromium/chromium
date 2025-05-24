// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"

#include <stddef.h>

#include <optional>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/views/tabs/tab_layout_state.h"
#include "chrome/browser/ui/views/tabs/tab_width_constraints.h"
#include "components/tabs/public/split_tab_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace {

// Returns a string with the width of each gfx::Rect in `tab_bounds`, separated
// by spaces.
std::string TabWidthsAsString(const std::vector<gfx::Rect>& tab_bounds) {
  std::string result;
  for (const auto& bounds : tab_bounds) {
    if (!result.empty()) {
      result += " ";
    }
    result += base::NumberToString(bounds.width());
  }
  return result;
}

// Returns a string with the x-coordinate of each gfx::Rect in `tab_bounds`,
// separated by spaces.
std::string TabXPositionsAsString(const std::vector<gfx::Rect>& tab_bounds) {
  std::string result;
  for (const auto& bounds : tab_bounds) {
    if (!result.empty()) {
      result += " ";
    }
    result += base::NumberToString(bounds.x());
  }
  return result;
}

struct TestCase {
  int num_pinned_tabs = 0;
  int num_tabs = 0;
  int active_index = 0;
  int tabstrip_width = 0;
  std::set<int> split_tabs;
};

constexpr int kStandardWidth = 256;
constexpr int kStandardSplitWidth = 137;
constexpr int kTabHeight = 41;
constexpr int kMinActiveWidth = 56;
constexpr int kMinActiveSplitWidth = 52;
constexpr int kMinInactiveWidth = 32;
constexpr int kPinnedWidth = 64;
constexpr int kPinnedSplitWidth = 55;
constexpr int kTabOverlap = 18;

std::vector<gfx::Rect> CalculateTabBounds(TestCase test_case) {
  TabSizeInfo size_info;
  size_info.pinned_tab_width = kPinnedWidth;
  size_info.min_active_width = kMinActiveWidth;
  size_info.min_inactive_width = kMinInactiveWidth;
  size_info.standard_width = kStandardWidth;

  TabSizeInfo split_size_info;
  split_size_info.pinned_tab_width = kPinnedSplitWidth;
  split_size_info.min_active_width = kMinActiveSplitWidth;
  split_size_info.min_inactive_width = kMinInactiveWidth;
  split_size_info.standard_width = kStandardSplitWidth;

  std::optional<split_tabs::SplitTabId> split_tab_id =
      split_tabs::SplitTabId::GenerateNew();

  std::vector<TabWidthConstraints> tab_states;
  for (int tab_index = 0; tab_index < test_case.num_tabs; tab_index++) {
    const bool is_split = test_case.split_tabs.contains(tab_index);
    TabLayoutState ideal_animation_state = TabLayoutState(
        TabOpen::kOpen,
        tab_index < test_case.num_pinned_tabs ? TabPinned::kPinned
                                              : TabPinned::kUnpinned,
        tab_index == test_case.active_index ? TabActive::kActive
                                            : TabActive::kInactive,
        is_split ? split_tab_id : std::nullopt);
    tab_states.emplace_back(ideal_animation_state,
                            is_split ? split_size_info : size_info);
  }

  return CalculateTabBounds(tab_states, test_case.tabstrip_width).first;
}

void ExpectTabsNarrowerThanTabStrip(const std::vector<gfx::Rect>& bounds,
                                    int tabstrip_width) {
  EXPECT_LT(bounds.back().right(), tabstrip_width);
}

void ExpectTabsFillTabStrip(const std::vector<gfx::Rect>& bounds,
                            int tabstrip_width) {
  EXPECT_EQ(bounds.back().right(), tabstrip_width);
}

void ExpectTabsWiderThanTabStrip(const std::vector<gfx::Rect>& bounds,
                                 int tabstrip_width) {
  EXPECT_GT(bounds.back().right(), tabstrip_width);
}

}  // namespace

// These tests verify that layout behaves correctly in various situations. In
// particular we want layout to adhere to the following constraints:
// * Tabs are the standard size given by TabSizeInfo when there's room.
// * Tabs are never smaller than the minimum sizes given by TabSizeInfo, even if
//   there isn't enough room.
// * Pinned tabs are always the width given by TabSizeInfo.
// * Remainder pixels (leftover when the available width is distributed evenly)
//   are distributed from left to right.
// * And otherwise tabs shrink to fit the available width.

TEST(TabStripLayoutTest, Basics) {
  TestCase test_case;
  test_case.tabstrip_width = 1000;
  test_case.num_tabs = 3;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("256 256 256", TabWidthsAsString(bounds));
  EXPECT_EQ("0 238 476", TabXPositionsAsString(bounds));
  for (const auto& b : bounds) {
    EXPECT_EQ(0, b.y());
    EXPECT_EQ(kTabHeight, b.height());
  }
  ExpectTabsNarrowerThanTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, AllPinnedTabs) {
  TestCase test_case;
  test_case.tabstrip_width = 1000;
  test_case.num_pinned_tabs = test_case.num_tabs = 3;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("64 64 64", TabWidthsAsString(bounds));
  EXPECT_EQ("0 46 92", TabXPositionsAsString(bounds));
  ExpectTabsNarrowerThanTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, MixedPinnedAndNormalTabs) {
  TestCase test_case;
  test_case.tabstrip_width = 1000;
  test_case.num_tabs = 3;
  test_case.num_pinned_tabs = 1;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("64 256 256", TabWidthsAsString(bounds));
  EXPECT_EQ("0 46 284", TabXPositionsAsString(bounds));
  ExpectTabsNarrowerThanTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, SplitPinnedTabs) {
  TestCase test_case;
  test_case.tabstrip_width = 1000;
  test_case.num_tabs = 2;
  test_case.num_pinned_tabs = 2;
  test_case.split_tabs = {0, 1};

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("55 55", TabWidthsAsString(bounds));
  EXPECT_EQ("0 37", TabXPositionsAsString(bounds));
  ExpectTabsNarrowerThanTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, MiddleWidth) {
  TestCase test_case;
  test_case.tabstrip_width = 598;
  test_case.num_tabs = 4;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("163 163 163 163", TabWidthsAsString(bounds));
  EXPECT_EQ("0 145 290 435", TabXPositionsAsString(bounds));
  ExpectTabsFillTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, MiddleWidthAndPinnedTab) {
  TestCase test_case;
  test_case.tabstrip_width = 400;
  test_case.num_tabs = 3;
  test_case.num_pinned_tabs = 1;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("64 186 186", TabWidthsAsString(bounds));
  EXPECT_EQ("0 46 214", TabXPositionsAsString(bounds));
  ExpectTabsFillTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, MiddleWidthRounded) {
  TestCase test_case;
  test_case.tabstrip_width = 600;
  test_case.num_tabs = 4;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("164 164 163 163", TabWidthsAsString(bounds));
  EXPECT_EQ("0 146 292 437", TabXPositionsAsString(bounds));
  ExpectTabsFillTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, MiddleWidthRoundedAndPinnedTab) {
  TestCase test_case;
  test_case.tabstrip_width = 401;
  test_case.num_tabs = 3;
  test_case.num_pinned_tabs = 1;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("64 187 186", TabWidthsAsString(bounds));
  EXPECT_EQ("0 46 215", TabXPositionsAsString(bounds));
  ExpectTabsFillTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, MiddleWidthRoundedAndSplitTab) {
  TestCase test_case;
  test_case.tabstrip_width = 602;
  test_case.num_tabs = 4;
  test_case.split_tabs = {0, 1};

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("117 117 211 211", TabWidthsAsString(bounds));
  ExpectTabsFillTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, MiddleWidthAndMinWidthSplitTab) {
  TestCase test_case;
  test_case.tabstrip_width = 138;
  test_case.num_tabs = 4;
  test_case.split_tabs = {0, 1};
  test_case.active_index = 2;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("44 44 56 48", TabWidthsAsString(bounds));
  ExpectTabsFillTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, BelowMinActiveWidth) {
  TestCase test_case;
  test_case.tabstrip_width = 196;
  test_case.num_tabs = 6;
  test_case.active_index = 3;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("46 46 46 56 46 46", TabWidthsAsString(bounds));
  EXPECT_EQ("0 28 56 84 122 150", TabXPositionsAsString(bounds));
  ExpectTabsFillTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, BelowMinActiveWidthRounded) {
  TestCase test_case;
  test_case.tabstrip_width = 200;
  test_case.num_tabs = 6;
  test_case.active_index = 3;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("47 47 47 56 47 46", TabWidthsAsString(bounds));
  ExpectTabsFillTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, BelowMinActiveWidthActivePinnedTab) {
  TestCase test_case;
  test_case.tabstrip_width = 249;
  test_case.num_tabs = 6;
  test_case.num_pinned_tabs = 1;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("64 55 55 55 55 55", TabWidthsAsString(bounds));
  ExpectTabsFillTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, BelowMinActiveWidthInactivePinnedTab) {
  TestCase test_case;
  test_case.tabstrip_width = 250;
  test_case.num_tabs = 6;
  test_case.num_pinned_tabs = 1;
  test_case.active_index = 2;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("64 55 56 55 55 55", TabWidthsAsString(bounds));
  ExpectTabsFillTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, BelowMinActiveWidthActivePinnedTabRounded) {
  TestCase test_case;
  test_case.tabstrip_width = 250;
  test_case.num_tabs = 6;
  test_case.num_pinned_tabs = 1;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("64 56 55 55 55 55", TabWidthsAsString(bounds));
  ExpectTabsFillTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, BelowMinActiveWidthSplitTab) {
  TestCase test_case;
  test_case.tabstrip_width = 200;
  test_case.num_tabs = 6;
  test_case.split_tabs = {0, 1};
  test_case.active_index = 2;

  // Can't avoid rounding with split tabs unless there is a large number of tabs
  // because regular tabs grow faster.
  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("45 45 56 48 48 48", TabWidthsAsString(bounds));
  ExpectTabsFillTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, NotEnoughSpace) {
  TestCase test_case;
  test_case.tabstrip_width = 10;
  test_case.num_tabs = 3;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("56 32 32", TabWidthsAsString(bounds));
  ExpectTabsWiderThanTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, NotEnoughSpaceOneTab) {
  TestCase test_case;
  test_case.tabstrip_width = 15;
  test_case.num_tabs = 1;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("56", TabWidthsAsString(bounds));
  EXPECT_EQ("0", TabXPositionsAsString(bounds));
  ExpectTabsWiderThanTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, NotEnoughSpaceAllPinnedTabs) {
  TestCase test_case;
  test_case.tabstrip_width = 10;
  test_case.num_tabs = 3;
  test_case.num_pinned_tabs = 3;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("64 64 64", TabWidthsAsString(bounds));
  ExpectTabsWiderThanTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, NotEnoughSpaceMixedPinnedAndNormalTabs) {
  TestCase test_case;
  test_case.tabstrip_width = 10;
  test_case.num_tabs = 3;
  test_case.num_pinned_tabs = 1;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("64 32 32", TabWidthsAsString(bounds));
  ExpectTabsWiderThanTabStrip(bounds, test_case.tabstrip_width);
}

TEST(TabStripLayoutTest, ExactlyEnoughSpaceAllPinnedTabs) {
  TestCase test_case;
  test_case.num_tabs = 2;
  test_case.num_pinned_tabs = 2;
  test_case.tabstrip_width = 2 * kPinnedWidth - kTabOverlap;

  // We want to check the case where the necessary strip width equals the
  // available width.
  auto bounds = CalculateTabBounds(test_case);

  EXPECT_EQ("64 64", TabWidthsAsString(bounds));

  // Validate that the tabstrip width is indeeed exactly enough to hold two
  // pinned tabs.
  ExpectTabsFillTabStrip(bounds, test_case.tabstrip_width);
}
