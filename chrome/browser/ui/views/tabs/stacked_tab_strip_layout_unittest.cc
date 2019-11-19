// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/stacked_tab_strip_layout.h"

#include <stddef.h>

#include <string>

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace {

struct CommonTestData {
  const int initial_x;
  const int width;
  const int tab_size;
  const int tab_overlap;
  const int stacked_offset;
  const int pinned_tab_count;
  const int active_index;
  const std::string start_bounds;
  const std::string expected_bounds;
};

}  // namespace

class StackedTabStripLayoutTest : public testing::Test {
 public:
  StackedTabStripLayoutTest() {}

 protected:
  void Reset(StackedTabStripLayout* layout,
             int x,
             int width,
             int pinned_tab_count,
             int active_index) {
    layout->Reset(x, width, pinned_tab_count, active_index);
  }

  void CreateLayout(const CommonTestData& data) {
    if (!data.start_bounds.empty())
      PrepareChildViewsFromString(data.start_bounds);
    else
      PrepareChildViewsFromString(data.expected_bounds);
    layout_ = std::make_unique<StackedTabStripLayout>(
        gfx::Size(data.tab_size, 10), data.tab_overlap, data.stacked_offset, 4,
        &view_model_);
    if (data.start_bounds.empty()) {
      PrepareChildViewsFromString(data.expected_bounds);
      layout_->Reset(data.initial_x, data.width, data.pinned_tab_count,
                     data.active_index);
    } else {
      ASSERT_NO_FATAL_FAILURE(SetBoundsFromString(data.start_bounds));
      layout_->Reset(data.initial_x, data.width, data.pinned_tab_count,
                     data.active_index);
      ASSERT_NO_FATAL_FAILURE(SetBoundsFromString(data.start_bounds));
    }
  }

  void AddViewToViewModel(int index) {
    views::View* child_view = new views::View;
    view_.AddChildView(child_view);
    view_model_.Add(child_view, index);
  }

  void PrepareChildViewsFromString(const std::string& bounds) {
    std::vector<base::StringPiece> positions = base::SplitStringPiece(
        bounds, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    PrepareChildViews(static_cast<int>(positions.size()));
  }

  void PrepareChildViews(int count) {
    view_model_.Clear();
    view_.RemoveAllChildViews(true);
    for (int i = 0; i < count; ++i)
      AddViewToViewModel(i);
  }

  void SetBoundsFromString(const std::string& bounds) {
    std::vector<base::StringPiece> positions = base::SplitStringPiece(
        bounds, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    PrepareChildViews(static_cast<int>(positions.size()));
    for (int i = 0; i < view_model_.view_size(); ++i) {
      int x = 0;
      gfx::Rect bounds(view_model_.ideal_bounds(i));
      ASSERT_TRUE(base::StringToInt(positions[i], &x));
      bounds.set_x(x);
      view_model_.set_ideal_bounds(i, bounds);
    }
  }

  std::string BoundsString() const {
    std::string result;
    for (int i = 0; i < view_model_.view_size(); ++i) {
      if (!result.empty())
        result += " ";
      result += base::NumberToString(view_model_.ideal_bounds(i).x());
    }
    return result;
  }

  std::string BoundsString2(int active_index) const {
    std::string result;
    for (int i = 0; i < view_model_.view_size(); ++i) {
      if (!result.empty())
        result += " ";
      if (i == active_index)
        result += "[";
      result += base::NumberToString(view_model_.ideal_bounds(i).x());
      if (i == active_index)
        result += "]";
    }
    return result;
  }

  int ideal_x(int index) const {
    return view_model_.ideal_bounds(index).x();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<StackedTabStripLayout> layout_;
  views::ViewModel view_model_;

 private:
  views::View view_;

  DISALLOW_COPY_AND_ASSIGN(StackedTabStripLayoutTest);
};

// Random data.
TEST_F(StackedTabStripLayoutTest, ValidateInitialLayout) {
  StackedTabStripLayout layout(gfx::Size(100, 10), 10, 2, 4, &view_model_);
  PrepareChildViews(12);

  for (int i = 120; i < 600; ++i) {
    for (int j = 0; j < 12; ++j) {
      Reset(&layout, 0, i, 0, j);
      for (int k = 1; k < view_model_.view_size(); ++k)
        EXPECT_LE(ideal_x(k) - ideal_x(k - 1), 90);
    }
  }
}

// Ensure initial layout is correct.
TEST_F(StackedTabStripLayoutTest, InitialLayout) {
  struct CommonTestData test_data[] = {
    { 0, 198, 100, 10, 1, 0, 9, "",
      "0 0 0 0 0 0 1 2 3 4 94 95 96 97 98 98 98 98" },
    { 0, 198, 100, 10, 1, 0, 0, "", "0 90 94 95 96 97 98 98 98" },
    { 0, 300, 100, 10, 1, 0, 0, "",
      "0 90 180 196 197 198 199 200 200 200 200" },
    { 0, 300, 100, 10, 1, 0, 10, "", "0 0 0 0 1 2 3 4 20 110 200" },
    { 0, 300, 100, 10, 1, 0, 1, "", "0 90 180 196 197 198 199 200 200" },
    { 0, 643, 160, 27, 6, 0, 0, "", "0 133 266 399" },
    { 0, 300, 100, 10, 1, 0, 7, "", "0 1 2 3 4 20 110 200" },
    { 0, 300, 100, 10, 1, 0, 6, "", "0 1 2 3 4 20 110 200" },
    { 0, 300, 100, 10, 1, 0, 4, "", "0 1 2 3 4 94 184 199 200" },
  };
  for (size_t i = 0; i < base::size(test_data); ++i) {
    CreateLayout(test_data[i]);
    EXPECT_EQ(test_data[i].expected_bounds, BoundsString()) << " at " << i;
  }
}

// Assertions for dragging from an existing configuration.
TEST_F(StackedTabStripLayoutTest, DragActiveTabExisting) {
  struct TestData {
    struct CommonTestData common_data;
    const int delta;
  } test_data[] = {
    //
    // The following set of tests create 6 tabs, the first two are pinned and
    // the 2nd tab is selected.
    //
    // 1 pixel to the right, should push only pinned tabs and first non-pinned
    // tab.
    { { 10, 240, 100, 10, 2, 2, 1, "0 5 10 100 138 140",
        "1 6 11 101 138 140" }, 1 },
    // Push enough to collapse the 4th tab.
    { { 10, 240, 100, 10, 2, 2, 1, "0 5 10 100 138 140",
        "36 41 46 136 138 140" }, 36 },
    // 1 past collapsing the 4th.
    { { 10, 240, 100, 10, 2, 2, 1, "0 5 10 100 138 140",
        "37 42 47 136 138 140" }, 37 },
    // Collapse the third.
    { { 10, 240, 100, 10, 2, 2, 1, "0 5 10 100 138 140",
        "124 129 134 136 138 140" }, 124 },
    // One past collapsing the third.
    { { 10, 240, 100, 10, 2, 2, 1, "0 5 10 100 138 140",
        "124 129 134 136 138 140" }, 125 },

    //
    // The following set of tests create 6 tabs, the first two are pinned and
    // the 5th is selected.
    //
    // 1 pixel to the right, should expose part of a tab.
    { { 10, 240, 100, 10, 2, 2, 4, "0 5 10 90 130 140", "0 5 10 90 131 140" },
      1 },
    // Push the tab as far to the right as it'll go.
    { { 10, 240, 100, 10, 2, 2, 4, "0 5 10 90 130 140", "0 5 10 90 138 140" },
      8 },
    // One past as far to the right as it'll go. Should expose more of the tab
    // before it.
    { { 10, 240, 100, 10, 2, 2, 4, "0 5 10 90 130 140", "0 5 10 91 138 140" },
      9 },
    // Enough so that the pinned tabs start pulling in.
    { { 10, 240, 100, 10, 2, 2, 4, "0 5 10 90 130 140", "1 6 11 101 138 140" },
      19 },
    // One more than last.
    { { 10, 240, 100, 10, 2, 2, 4, "0 5 10 90 130 140", "2 7 12 102 138 140" },
      20 },
    // Enough to collapse the fourth as small it can get.
    { { 10, 240, 100, 10, 2, 2, 4, "0 5 10 90 130 140",
        "36 41 46 136 138 140" }, 54 },
    // Enough to collapse the third as small it can get.
    { { 10, 240, 100, 10, 2, 2, 4, "0 5 10 90 130 140",
        "124 129 134 136 138 140" }, 142 },
    // One more than last, shouldn't change anything.
    { { 10, 240, 100, 10, 2, 2, 4, "0 5 10 90 130 140",
        "124 129 134 136 138 140" }, 143 },

    //
    // The following set of tests create 3 tabs with the second selected.
    //
    // Drags in 2, pulling the rightmost tab along.
    { { 0, 240, 100, 10, 2, 0, 1, "0 90 140", "2 92 140" }, 2 },
    // Drags the rightmost tab as far to right as possible.
    { { 0, 240, 100, 10, 2, 0, 1, "0 90 140", "48 138 140" }, 48 },
    // Drags so much that the left most tabs pulls in.
    { { 0, 240, 100, 10, 2, 0, 1, "0 90 140", "135 138 140" }, 135 },
    // Drags so far that no more tabs pull in.
    { { 0, 240, 100, 10, 2, 0, 1, "0 90 140", "136 138 140" }, 200 },
    // Drags to the left most position before the right tabs start pulling in.
    { { 0, 240, 100, 10, 2, 0, 1, "0 90 140", "0 50 140" }, -40 },
    // Drags 1 beyond the left most position, which should pull in the right
    // tab slightly.
    { { 0, 240, 100, 10, 2, 0, 1, "0 90 140", "0 49 139" }, -41 },
    // Drags to the left as far as the tab goes.
    { { 0, 240, 100, 10, 2, 0, 1, "0 90 140", "0 2 92" }, -88 },
    // Drags one past as far to the left as the tab goes. Should keep pulling
    // in the rightmost tab.
    { { 0, 240, 100, 10, 2, 0, 1, "0 90 140", "0 2 91" }, -89 },

    //
    // The following set of tests create six tabs with the third selected.
    //
    // The x-position of the third tab is at its maximum, and the second tab is
    // stacked underneath. Dragging to the left moves the second tab to the
    // stack at the left-hand side of the tab strip.
    { { 0, 150, 100, 10, 2, 0, 2, "0 42 44 46 48 50", "0 2 43 46 48 50" }, -1 },
    { { 0, 150, 100, 10, 2, 0, 2, "0 20 44 46 48 50", "0 2 41 46 48 50" }, -3 },
    // The x-position of the third tab is not at its maximum. Dragging to the
    // left moves the second and third tabs by the same delta.
    { { 0, 150, 100, 10, 2, 0, 2, "0 25 35 46 48 50", "0 20 30 46 48 50" },
      -5 },
    // min x, fourth is flush against right side
    // The x-position of the third tab is at its minimum, and the fourth tab is
    // stacked underneath. Dragging to the right moves the fourth and fifth tabs
    // to the stack at the right-hand side of the tab strip.
    { { 0, 150, 100, 10, 2, 0, 2, "0 2 4 6 25 50", "0 2 11 46 48 50" }, 7 },
    { { 0, 150, 100, 10, 2, 0, 2, "0 2 4 9 25 50", "0 2 7 46 48 50" }, 3 },
    // The x-position of the third tab is not at its minimum. Dragging to the
    // right moves the third, fourth, and fifth tabs by the same delta.
    { { 0, 150, 100, 10, 2, 0, 2, "0 2 10 16 25 50", "0 2 11 17 26 50" }, 1 },
  };

  for (size_t i = 0; i < base::size(test_data); ++i) {
    CreateLayout(test_data[i].common_data);
    layout_->DragActiveTab(test_data[i].delta);
    EXPECT_EQ(test_data[i].common_data.expected_bounds, BoundsString()) <<
        " at " << i;
  }
}

// Assertions for SizeToFit().
TEST_F(StackedTabStripLayoutTest, SizeToFit) {
  struct CommonTestData test_data[] = {
    // Dragged to the right.
    { 10, 240, 100, 10, 2, 2, 1, "0 5 10 100 138 140", "1 6 11 101 138 140"},
    { 10, 240, 100, 10, 2, 2, 1, "0 5 10 100 138 140",
      "124 129 134 136 138 140" },

    // Dragged to the left.
    { 0, 240, 100, 10, 2, 0, 1, "0 50 140", "0 49 139" },

    // Dragged to the left.
    { 0, 240, 100, 10, 2, 0, 1, "0 49 89 140", "0 49 89 139" },
  };

  for (size_t i = 0; i < base::size(test_data); ++i) {
    CreateLayout(test_data[i]);
    SetBoundsFromString(test_data[i].expected_bounds);
    layout_->SizeToFit();
    // NOTE: because of the way the code is structured this asserts on
    // |start_bound|, not |expected_bounds|.
    EXPECT_EQ(test_data[i].start_bounds, BoundsString()) << " at " << i;
  }
}

// Assertions for AddTab().
TEST_F(StackedTabStripLayoutTest, AddTab) {
  struct TestData {
    CommonTestData common_data;
    int add_index;
    bool add_active;
    bool add_pinned;
  } test_data[] = {
    { { 0, 300, 100, 10, 2, 0, 1, "0 90 180 198 200", "0 16 106 196 198 200"},
      3, false, false },

    // If the active tab is in its leftmost position and it is not possible
    // for all of the tabs between the active tab and the newly-added tab
    // (inclusive) to be shown, then a stack should form to the right of
    // the active tab.
    { { 0, 284, 100, 10, 2, 0, 2, "0 2 4 94 184", "0 2 4 6 94 184"},
      5, false, false },
    { { 0, 300, 100, 10, 2, 0, 1, "0 90 180 198 200", "0 2 4 20 110 200"},
      5, false, false },

    { { 0, 300, 100, 10, 2, 0, 1, "0 90 180 198 200", "0 90 180 196 198 200"},
      2, false, false },

    // Add to the end of the tab strip. All tabs between the active tab and the
    // newly-added tab (inclusive) should be fully visible (indices 3-5 in the
    // resulting tab strip) and tabs to the left of the active tab should be
    // stacked at the left side of the tab strip rather than immediately to the
    // left of the active tab.
    { { 0, 300, 100, 10, 2, 0, 3, "0 90 180 198 200", "0 2 4 20 110 200"},
      5, false, false },

    // If it is possible for all of the tabs between the active tab and the
    // newly-added tab (inclusive) to be fully visible without changing the
    // position of the active tab, then do not do so.
    { { 0, 378, 100, 10, 2, 0, 2, "0 2 4 94 184 274 276 278",
                                  "0 2 4 94 184 272 274 276 278"},
      3, false, false },
    { { 0, 378, 100, 10, 2, 0, 2, "0 2 4 94 184 274 276 278",
                                  "0 2 4 94 184 272 274 276 278"},
      4, false, false },

    { { 4, 200, 100, 10, 2, 1, 2, "0 4 10 100", "0 0 8 10 100"},
      1, false, true },
    { { 4, 200, 100, 10, 2, 1, 2, "0 4 10 100", "0 0 8 98 100"},
      1, true, true },
    { { 4, 200, 100, 10, 2, 1, 2, "0 4 10 100", "0 0 8 98 100"},
      0, true, true },
    { { 0, 200, 100, 10, 2, 0, 2, "0 2 10 100", "0 4 94 98 100"},
      0, true, true },

    { { 0, 200, 100, 10, 2, 0, 0, "0 90 92 92 94 96 98 100",
                                  "0 0 0 2 4 6 8 98 100"},
      7, true, false },
    { { 0, 200, 100, 10, 2, 0, 7, "0 2 4 6 8 8 10 100",
                                  "0 0 2 4 6 8 96 98 100"},
      5, true, false },
    { { 0, 200, 100, 10, 2, 0, 7, "0 2 4 6 8 8 10 100",
                                  "0 2 4 6 8 94 96 98 100"},
      4, true, false },
    { { 0, 200, 100, 10, 2, 0, 2, "0 2 10 100", "0 2 10 98 100"},
      2, true, false },
    { { 0, 200, 100, 10, 2, 0, 2, "0 2 10 100", "0 2 4 10 100"},
      4, true, false },
    { { 0, 200, 100, 10, 2, 0, 2, "0 2 10 100", "0 90 96 98 100"},
      0, true, false },
  };
  for (size_t i = 0; i < base::size(test_data); ++i) {
    CreateLayout(test_data[i].common_data);
    int add_types = 0;
    if (test_data[i].add_active)
      add_types |= StackedTabStripLayout::kAddTypeActive;
    if (test_data[i].add_pinned)
      add_types |= StackedTabStripLayout::kAddTypePinned;
    AddViewToViewModel(test_data[i].add_index);
    layout_->AddTab(test_data[i].add_index, add_types,
                    test_data[i].common_data.initial_x +
                    (test_data[i].add_pinned ? 4 : 0));
    EXPECT_EQ(test_data[i].common_data.expected_bounds, BoundsString()) <<
        " at " << i;
  }
}

// Assertions around removing tabs.
TEST_F(StackedTabStripLayoutTest, RemoveTab) {
  // TODO: add coverage of removing pinned tabs!
  struct TestData {
    struct CommonTestData common_data;
    const int remove_index;
    const int x_after_remove;
  } test_data[] = {
    { { 0, 882, 220, 29, 2, 0, 4, "0 23 214 405 596 602",
        "0 191 382 573 662" }, 1, 0 },

    // Remove before active.
    { { 0, 200, 100, 10, 2, 0, 4, "0 2 4 6 8 10 80 98 100",
        "0 2 6 8 10 80 98 100" },
      2, 0 },

    // Stacked tabs on both sides.
    { { 0, 200, 100, 10, 2, 0, 4, "0 2 4 6 8 10 80 98 100",
        "0 2 4 6 10 80 98 100" },
      4, 0 },

    // Pinned tabs.
    { { 8, 200, 100, 10, 2, 1, 0, "0 8 94 96 98 100", "0 86 88 90 100" },
      0, 0 },
    { { 16, 200, 100, 10, 2, 2, 0, "0 8 16 94 96 98 100", "8 8 86 88 90 100" },
      0, 8 },
    { { 16, 200, 100, 10, 2, 2, 0, "0 8 16 94 96 98 100", "0 8 86 88 90 100" },
      1, 8 },

    // Remove from ideal layout.
    { { 0, 200, 100, 10, 2, 0, 0, "0 90 94 96 98 100", "0 90 96 98 100" },
      0, 0 },
    { { 0, 200, 100, 10, 2, 0, 0, "0 90 94 96 98 100", "0 90 96 98 100" },
      1, 0 },
    { { 0, 200, 100, 10, 2, 0, 0, "0 90 94 96 98 100", "0 90 96 98 100" },
      2, 0 },
    { { 0, 200, 100, 10, 2, 0, 0, "0 90 94 96 98 100", "0 90 94 98 100" },
      3, 0 },
    { { 0, 200, 100, 10, 2, 0, 0, "0 90 94 96 98 100", "0 90 94 96 100" },
      5, 0 },
  };
  for (size_t i = 0; i < base::size(test_data); ++i) {
    CreateLayout(test_data[i].common_data);
    int old_x = view_model_.ideal_bounds(test_data[i].remove_index).x();
    view_model_.Remove(test_data[i].remove_index);
    layout_->RemoveTab(test_data[i].remove_index, test_data[i].x_after_remove,
                       old_x);
    EXPECT_EQ(test_data[i].common_data.expected_bounds, BoundsString()) <<
        " at " << i;
  }
}

// Assertions for SetWidth().
TEST_F(StackedTabStripLayoutTest, SetWidth) {
  struct TestData {
    CommonTestData common_data;
    int new_width;
  } test_data[] = {

    // No change in layout if SetWidth() is called with current width.
    { { 0, 500, 100, 10, 2, 0, 4, "", "0 90 180 270 360 400"}, 500 },

    // No change in layout if stacking is initially not required and the tab
    // strip width is increased.
    { { 0, 500, 100, 10, 2, 0, 2, "", "0 90 180"}, 550 },

    // For an initially non-stacked tab strip whose width is being decreased,
    // only start to stack once the width becomes narrow enough.
    { { 0, 500, 100, 10, 2, 0, 2, "", "0 90 180"}, 400 },
    { { 0, 500, 100, 10, 2, 0, 2, "0 90 180", "0 10 100"}, 200 },

    // Increase a stacked tabstrip width enough so that stacking is no longer
    // required.
    { { 0, 200, 100, 10, 2, 0, 2, "0 10 100", "0 90 180"}, 400 },

    // Verifies a bug in AdjustTrailingStackedTabs(). See crbug.com/125127.
    { { 0, 103, 100, 10, 2, 0, 0, "", "0 2"}, 102 },

    // Tests with pinned tabs.
    { { 8, 250, 100, 10, 2, 2, 2, "0 4 8 98 148 150", "0 4 8 98 188 250"},
      350 },
    { { 8, 250, 100, 10, 2, 2, 2, "0 4 8 98 148 150", "0 4 8 96 98 100"}, 200 },

    // Decrease the width of the tabstrip by a small enough amount such that
    // tabs to the right of the active tab form a stack and the positions of
    // all other tabs remain the same.
    { { 0, 500, 100, 10, 2, 0, 4, "0 90 180 270 360 400",
                                  "0 90 180 270 360 390"}, 490},
    { { 0, 500, 100, 10, 2, 0, 0, "0 90 180 270 360 400",
                                  "0 90 180 196 198 200"}, 300},
    { { 0, 500, 100, 10, 2, 0, 2, "0 90 180 270 360 400",
                                  "0 90 180 270 298 300"}, 400},

    // Decrease the width of the tabstrip by a large enough amount such that
    // all tabs to the right of the active tab stack, the active tab changes
    // position, and the tabs to the left of the active tab start to stack.
    { { 0, 500, 100, 10, 2, 0, 4, "0 90 180 270 360 400",
                                  "0 2 18 108 198 200"}, 300 },
    { { 0, 500, 100, 10, 2, 0, 5, "0 90 180 270 360 400",
                                  "0 2 18 108 198 288"}, 388 },
    { { 0, 500, 100, 10, 2, 0, 2, "0 90 180 270 360 400",
                                  "0 54 144 146 148 150"}, 250 },

    // Increase the width of the tabstrip by a small enough amount such that
    // the tabs to the right of the active start to become exposed and the rest
    // of the tabs do not change position.
    { { 0, 350, 100, 10, 2, 0, 2, "0 20 110 200 250",
                                  "0 20 110 200 260"}, 360 },
    { { 0, 350, 100, 10, 2, 0, 2, "0 20 110 200 250",
                                  "0 20 110 200 290"}, 390 },
    { { 0, 110, 100, 10, 2, 0, 3, "0 2 4 6 8 10",
                                  "0 2 4 6 48 50"}, 150 },
    { { 0, 110, 100, 10, 2, 0, 3, "0 2 4 6 8 10",
                                  "0 2 4 6 96 110"}, 210 },

    // Increase the width of the tabstrip by a large enough amount such that
    // all tabs to the right of the active tab are fully exposed, the active
    // tab changes position, and the tabs to the left of the active tab start
    // to become exposed.
    { { 0, 350, 100, 10, 2, 0, 2, "0 20 110 200 250",
                                  "0 30 120 210 300"}, 400 },
    { { 0, 110, 100, 10, 2, 0, 3, "0 2 4 6 8 10",
                                  "0 2 4 94 184 274"}, 374 },
    { { 0, 110, 100, 10, 2, 0, 3, "0 2 4 6 8 10",
                                  "0 2 5 95 185 275"}, 375 },
    { { 0, 110, 100, 10, 2, 0, 3, "0 2 4 6 8 10",
                                  "0 40 130 220 310 400"}, 500 },

    // If a stack has formed somewhere other than at the very beginning or
    // very end of the tabstrip (possible as a result of a gesture overscroll),
    // then re-adjust stacking to the ends of the tabstrip upon a tabstrip
    // width change.
    { { 0, 200, 100, 10, 2, 0, 2, "0 90 92 100", "0 2 78 80"}, 180 },

  };
  for (size_t i = 0; i < base::size(test_data); ++i) {
    CreateLayout(test_data[i].common_data);
    layout_->SetWidth(test_data[i].new_width);
    EXPECT_EQ(test_data[i].common_data.expected_bounds, BoundsString()) <<
        " at " << i;
  }
}

// Assertions for SetActiveIndex().
TEST_F(StackedTabStripLayoutTest, SetActiveIndex) {
  struct TestData {
    CommonTestData common_data;
    int new_index;
  } test_data[] = {
    { { 0, 250, 100, 10, 2, 0, 2, "0 4 8 98 148 150", "0 90 144 146 148 150"},
      0 },
    { { 0, 250, 100, 10, 2, 0, 2, "0 4 8 98 148 150", "0 2 4 58 148 150"}, 4 },
    { { 0, 250, 100, 10, 2, 0, 2, "0 4 8 98 148 150", "0 2 4 6 60 150"}, 5 },
    { { 4, 250, 100, 10, 2, 1, 2, "0 4 8 98 148 150", "0 4 94 146 148 150"},
      0 },
  };
  for (size_t i = 0; i < base::size(test_data); ++i) {
    CreateLayout(test_data[i].common_data);
    layout_->SetActiveIndex(test_data[i].new_index);
    EXPECT_EQ(test_data[i].common_data.expected_bounds, BoundsString()) <<
        " at " << i;
  }
}

// Makes sure don't crash when resized and only one tab.
TEST_F(StackedTabStripLayoutTest, EmptyTest) {
  StackedTabStripLayout layout(gfx::Size(160, 10), 27, 6, 4, &view_model_);
  PrepareChildViews(1);
  layout.AddTab(0, StackedTabStripLayout::kAddTypeActive, 0);
  layout.SetWidth(100);
  layout.SetWidth(50);
  layout.SetWidth(0);
  layout.SetWidth(500);
}

// Assertions around removing tabs.
TEST_F(StackedTabStripLayoutTest, MoveTab) {
  // TODO: add coverage of removing pinned tabs!
  struct TestData {
    struct CommonTestData common_data;
    const int from;
    const int to;
    const int new_active_index;
    const int new_start_x;
    const int new_pinned_tab_count;
  } test_data[] = {
    // Moves and unpins.
    { { 10, 300, 100, 10, 2, 2, 0, "", "0 5 10 100 190 198 200" },
      0, 1, 2, 5, 1 },

    // Moves and pins.
    { { 0, 300, 100, 10, 2, 0, 4, "", "0 5 95 185 196 198 200" },
      2, 0, 0, 5, 1 },
    { { 0, 300, 100, 10, 2, 1, 2, "", "0 5 10 100 190 198 200" },
      2, 0, 0, 10, 2 },

    { { 0, 200, 100, 10, 2, 0, 4, "0 2 4 6 96 98 100", "0 2 4 6 96 98 100" },
      2, 0, 4, 0, 0 },
    { { 0, 200, 100, 10, 2, 0, 4, "0 2 4 6 96 98 100", "0 2 4 6 8 10 100" },
      0, 6, 6, 0, 0 },
  };
  for (size_t i = 0; i < base::size(test_data); ++i) {
    CreateLayout(test_data[i].common_data);
    view_model_.MoveViewOnly(test_data[i].from, test_data[i].to);
    for (int j = 0; j < test_data[i].new_pinned_tab_count; ++j) {
      gfx::Rect bounds;
      bounds.set_x(j * 5);
      view_model_.set_ideal_bounds(j, bounds);
    }
    layout_->MoveTab(test_data[i].from, test_data[i].to,
                     test_data[i].new_active_index, test_data[i].new_start_x,
                     test_data[i].new_pinned_tab_count);
    EXPECT_EQ(test_data[i].common_data.expected_bounds, BoundsString()) <<
        " at " << i;
  }
}

// Assertions around IsStacked().
TEST_F(StackedTabStripLayoutTest, IsStacked) {
  // A single tab with enough space should never be stacked.
  PrepareChildViews(1);
  layout_.reset(
      new StackedTabStripLayout(gfx::Size(100, 10), 10, 2, 4, &view_model_));
  Reset(layout_.get(), 0, 400, 0, 0);
  EXPECT_FALSE(layout_->IsStacked(0));

  // First tab active, remaining tabs stacked.
  PrepareChildViews(8);
  Reset(layout_.get(), 0, 400, 0, 0);
  EXPECT_FALSE(layout_->IsStacked(0));
  EXPECT_TRUE(layout_->IsStacked(7));

  // Last tab active, preceeding tabs stacked.
  layout_->SetActiveIndex(7);
  EXPECT_FALSE(layout_->IsStacked(7));
  EXPECT_TRUE(layout_->IsStacked(0));
}

// Assertions around SetXAndPinnedCount.
TEST_F(StackedTabStripLayoutTest, SetXAndPinnedCount) {
  // Verifies we don't crash when transitioning to all pinned tabs.
  PrepareChildViews(1);
  layout_.reset(
      new StackedTabStripLayout(gfx::Size(100, 10), 10, 2, 4, &view_model_));
  Reset(layout_.get(), 0, 400, 0, 0);
  layout_->SetXAndPinnedCount(0, 1);
}

// Assertions around SetXAndPinnedCount.
TEST_F(StackedTabStripLayoutTest, SetActiveTabLocation) {
  struct TestData {
    struct CommonTestData common_data;
    const int location;
  } test_data[] = {
    // Active tab is the first tab, can't be moved.
    { { 0, 300, 100, 10, 2, 0, 0, "", "0 90 180 194 196 198 200" }, 50 },

    // Active tab is pinned; should result in nothing.
    { { 0, 300, 100, 10, 2, 2, 1, "", "0 0 0 90 180 198 200" }, 199 },

    // Location is too far to the right, ends up being pushed in.
    { { 0, 300, 100, 10, 2, 0, 3, "", "0 14 104 194 196 198 200" }, 199 },

    // Location can be honored.
    { { 0, 300, 100, 10, 2, 0, 3, "", "0 2 4 40 130 198 200" }, 40 },
  };
  for (size_t i = 0; i < base::size(test_data); ++i) {
    CreateLayout(test_data[i].common_data);
    layout_->SetActiveTabLocation(test_data[i].location);
    EXPECT_EQ(test_data[i].common_data.expected_bounds, BoundsString()) <<
        " at " << i;
  }
}
