// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"

#include <stddef.h>
#include <string>

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/views/tabs/tab_animation_state.h"
#include "chrome/browser/ui/views/tabs/tab_width_constraints.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace {

// Returns a string with the width of each gfx::Rect in |tab_bounds|, separated
// by spaces.
std::string TabWidthsAsString(const std::vector<gfx::Rect>& tab_bounds) {
  std::string result;
  for (const auto& bounds : tab_bounds) {
    if (!result.empty())
      result += " ";
    result += base::NumberToString(bounds.width());
  }
  return result;
}

// Returns a string with the x-coordinate of each gfx::Rect in |tab_bounds|,
// separated by spaces.
std::string TabXPositionsAsString(const std::vector<gfx::Rect>& tab_bounds) {
  std::string result;
  for (const auto& bounds : tab_bounds) {
    if (!result.empty())
      result += " ";
    result += base::NumberToString(bounds.x());
  }
  return result;
}

struct TestCase {
  int num_pinned_tabs = 0;
  int num_tabs = 0;
  int active_index = 0;
  int tabstrip_width = 0;
};

constexpr int kStandardWidth = 100;
constexpr int kTabHeight = 10;
constexpr int kMinActiveWidth = 20;
constexpr int kMinInactiveWidth = 14;
constexpr int kPinnedWidth = 10;
constexpr int kTabOverlap = 4;

std::vector<gfx::Rect> CalculateTabBounds(TestCase test_case) {
  TabLayoutConstants layout_constants;
  layout_constants.tab_height = kTabHeight;
  layout_constants.tab_overlap = kTabOverlap;

  TabSizeInfo size_info;
  size_info.pinned_tab_width = kPinnedWidth;
  size_info.min_active_width = kMinActiveWidth;
  size_info.min_inactive_width = kMinInactiveWidth;
  size_info.standard_width = kStandardWidth;

  std::vector<TabWidthConstraints> tab_states;
  for (int tab_index = 0; tab_index < test_case.num_tabs; tab_index++) {
    TabAnimationState ideal_animation_state =
        TabAnimationState::ForIdealTabState(
            TabOpen::kOpen,
            tab_index < test_case.num_pinned_tabs ? TabPinned::kPinned
                                                  : TabPinned::kUnpinned,
            tab_index == test_case.active_index ? TabActive::kActive
                                                : TabActive::kInactive,
            0);
    tab_states.push_back(TabWidthConstraints(ideal_animation_state,
                                             layout_constants, size_info));
  }

  return CalculateTabBounds(layout_constants, tab_states,
                            test_case.tabstrip_width, base::nullopt);
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
  EXPECT_EQ("100 100 100", TabWidthsAsString(bounds));
  EXPECT_EQ("0 96 192", TabXPositionsAsString(bounds));
  for (const auto& b : bounds) {
    EXPECT_EQ(0, b.y());
    EXPECT_EQ(kTabHeight, b.height());
  }
}

TEST(TabStripLayoutTest, AllPinnedTabs) {
  TestCase test_case;
  test_case.tabstrip_width = 1000;
  test_case.num_pinned_tabs = test_case.num_tabs = 3;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("10 10 10", TabWidthsAsString(bounds));
  EXPECT_EQ("0 6 12", TabXPositionsAsString(bounds));
}

TEST(TabStripLayoutTest, MixedPinnedAndNormalTabs) {
  TestCase test_case;
  test_case.tabstrip_width = 1000;
  test_case.num_tabs = 3;
  test_case.num_pinned_tabs = 1;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("10 100 100", TabWidthsAsString(bounds));
  EXPECT_EQ("0 6 102", TabXPositionsAsString(bounds));
}

TEST(TabStripLayoutTest, MiddleWidth) {
  TestCase test_case;
  test_case.tabstrip_width = 100;
  test_case.num_tabs = 4;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("28 28 28 28", TabWidthsAsString(bounds));
  EXPECT_EQ("0 24 48 72", TabXPositionsAsString(bounds));
}

TEST(TabStripLayoutTest, MiddleWidthAndPinnedTab) {
  TestCase test_case;
  test_case.tabstrip_width = 100;
  test_case.num_tabs = 3;
  test_case.num_pinned_tabs = 1;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("10 49 49", TabWidthsAsString(bounds));
  EXPECT_EQ("0 6 51", TabXPositionsAsString(bounds));
}

TEST(TabStripLayoutTest, MiddleWidthRounded) {
  TestCase test_case;
  test_case.tabstrip_width = 102;
  test_case.num_tabs = 4;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("29 29 28 28", TabWidthsAsString(bounds));
  EXPECT_EQ("0 25 50 74", TabXPositionsAsString(bounds));
}

TEST(TabStripLayoutTest, MiddleWidthRoundedAndPinnedTab) {
  TestCase test_case;
  test_case.tabstrip_width = 101;
  test_case.num_tabs = 3;
  test_case.num_pinned_tabs = 1;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("10 50 49", TabWidthsAsString(bounds));
  EXPECT_EQ("0 6 52", TabXPositionsAsString(bounds));
}

TEST(TabStripLayoutTest, BelowMinActiveWidthOneTab) {
  TestCase test_case;
  test_case.tabstrip_width = 15;
  test_case.num_tabs = 1;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("20", TabWidthsAsString(bounds));
  EXPECT_EQ("0", TabXPositionsAsString(bounds));
}

TEST(TabStripLayoutTest, BelowMinActiveWidth) {
  TestCase test_case;
  test_case.tabstrip_width = 90;
  test_case.num_tabs = 6;
  test_case.active_index = 3;

  auto bounds = CalculateTabBounds(test_case);
  EXPECT_EQ("18 18 18 20 18 18", TabWidthsAsString(bounds));
  EXPECT_EQ("0 14 28 42 58 72", TabXPositionsAsString(bounds));
}

TEST(TabStripLayoutTest, BelowMinActiveWidthRounded) {
  TestCase test_case;
  test_case.tabstrip_width = 93;
  test_case.num_tabs = 6;
  test_case.active_index = 3;

  EXPECT_EQ("19 19 19 20 18 18",
            TabWidthsAsString(CalculateTabBounds(test_case)));
}

TEST(TabStripLayoutTest, BelowMinActiveWidthActivePinnedTab) {
  TestCase test_case;
  test_case.tabstrip_width = 85;
  test_case.num_tabs = 6;
  test_case.num_pinned_tabs = 1;

  EXPECT_EQ("10 19 19 19 19 19",
            TabWidthsAsString(CalculateTabBounds(test_case)));
}

TEST(TabStripLayoutTest, BelowMinActiveWidthInactivePinnedTab) {
  TestCase test_case;
  test_case.tabstrip_width = 82;
  test_case.num_tabs = 6;
  test_case.num_pinned_tabs = 1;
  test_case.active_index = 2;

  EXPECT_EQ("10 18 20 18 18 18",
            TabWidthsAsString(CalculateTabBounds(test_case)));
}

TEST(TabStripLayoutTest, BelowMinActiveWidthActivePinnedTabRounded) {
  TestCase test_case;
  test_case.tabstrip_width = 86;
  test_case.num_tabs = 6;
  test_case.num_pinned_tabs = 1;

  EXPECT_EQ("10 20 19 19 19 19",
            TabWidthsAsString(CalculateTabBounds(test_case)));
}

TEST(TabStripLayoutTest, NotEnoughSpace) {
  TestCase test_case;
  test_case.tabstrip_width = 10;
  test_case.num_tabs = 3;

  EXPECT_EQ("20 14 14", TabWidthsAsString(CalculateTabBounds(test_case)));
}

TEST(TabStripLayoutTest, NotEnoughSpaceAllPinnedTabs) {
  TestCase test_case;
  test_case.tabstrip_width = 10;
  test_case.num_tabs = 3;
  test_case.num_pinned_tabs = 3;

  EXPECT_EQ("10 10 10", TabWidthsAsString(CalculateTabBounds(test_case)));
}

TEST(TabStripLayoutTest, NotEnoughSpaceMixedPinnedAndNormalTabs) {
  TestCase test_case;
  test_case.tabstrip_width = 10;
  test_case.num_tabs = 3;
  test_case.num_pinned_tabs = 1;

  EXPECT_EQ("10 14 14", TabWidthsAsString(CalculateTabBounds(test_case)));
}

TEST(TabStripLayoutTest, ExactlyEnoughSpaceAllPinnedTabs) {
  TestCase test_case;
  test_case.num_tabs = 2;
  test_case.num_pinned_tabs = 2;
  test_case.tabstrip_width = 2 * kPinnedWidth - kTabOverlap;

  // We want to check the case where the necessary strip width equals the
  // available width.
  auto bounds = CalculateTabBounds(test_case);

  EXPECT_EQ("10 10", TabWidthsAsString(bounds));

  // Validate that the tabstrip width is indeeed exactly enough to hold two
  // pinned tabs.
  EXPECT_EQ(test_case.tabstrip_width, bounds[1].right());
}
