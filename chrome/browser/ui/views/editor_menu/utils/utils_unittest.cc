// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "utils.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/test_screen.h"

using display::test::TestScreen;

using ::testing::TestWithParam;
using ::testing::ValuesIn;

namespace chromeos::editor_menu {

namespace {

constexpr int kEditorMenuHeightForTestDip = 180;
constexpr int kSmallGapForTestDip = 5;

struct GetEditorMenuBoundsTestParams {
  gfx::Point cursor_point;
  gfx::Rect anchor_view_bounds;
  gfx::Rect editor_menu_bounds;
};

std::vector<GetEditorMenuBoundsTestParams> get_editor_menu_bounds_test_cases = {
    // When:
    //  - Context menu covers left half of the screen.
    //  - Cursor point is on the right side of context menu (vertically middle).
    // Then:
    //  - Editor menu appears on the right bottom side of cursor point.
    //  - Also it's moved vertically closer to cursor point.
    {
        .cursor_point = gfx::Point(
            /*x=*/TestScreen::kDefaultScreenBounds.width() / 2 +
                kSmallGapForTestDip,
            /*y=*/TestScreen::kDefaultScreenBounds.height() / 2),
        .anchor_view_bounds =
            {
                gfx::Point(
                    /*x=*/0,
                    /*y=*/0),
                gfx::Size(
                    /*width=*/TestScreen::kDefaultScreenBounds.width() / 2,
                    /*height=*/TestScreen::kDefaultScreenBounds.height()),
            },
        .editor_menu_bounds =
            {
                gfx::Point(
                    /*x=*/TestScreen::kDefaultScreenBounds.width() / 2 +
                        kEditorMenuMarginDip,
                    /*y=*/TestScreen::kDefaultScreenBounds.height() / 2 +
                        kEditorMenuMarginDip),
                gfx::Size(
                    /*width=*/kEditorMenuMinWidthDip,
                    /*height=*/kEditorMenuHeightForTestDip),
            },
    },
    // When:
    //  - Context menu covers right half of the screen.
    //  - Cursor point is on left side of context menu (verticlly middle).
    // Then:
    //  - Editor menu appears on the left bottom side of cursor point.
    //  - Also it's moved vertically closer to cursor point.
    {
        .cursor_point = gfx::Point(
            /*x=*/TestScreen::kDefaultScreenBounds.width() / 2 -
                kSmallGapForTestDip,
            /*y=*/TestScreen::kDefaultScreenBounds.height() / 2),
        .anchor_view_bounds =
            {
                gfx::Point(
                    /*x=*/TestScreen::kDefaultScreenBounds.width() / 2,
                    /*y=*/0),
                gfx::Size(
                    /*width=*/TestScreen::kDefaultScreenBounds.width() / 2,
                    /*height=*/TestScreen::kDefaultScreenBounds.height()),
            },
        .editor_menu_bounds =
            {gfx::Point(
                 /*x=*/TestScreen::kDefaultScreenBounds.width() / 2 -
                     kEditorMenuMarginDip - kEditorMenuMinWidthDip,
                 /*y=*/TestScreen::kDefaultScreenBounds.height() / 2 +
                     kEditorMenuMarginDip),
             gfx::Size(
                 /*width=*/kEditorMenuMinWidthDip,
                 /*height=*/kEditorMenuHeightForTestDip)},
    },
    // When:
    //   - Context menu leaves enough space on every side.
    //   - Cursor point is above context menu.
    // Then:
    //   - Editor menu appears above context menu.
    {
        .cursor_point = gfx::Point(
            /*x=*/TestScreen::kDefaultScreenBounds.width() / 2,
            /*y=*/0),
        .anchor_view_bounds =
            {
                gfx::Point(
                    /*x=*/kEditorMenuMinWidthDip + kEditorMenuMarginDip,
                    /*y=*/kEditorMenuHeightForTestDip + kEditorMenuMarginDip),
                gfx::Size(
                    /*width=*/TestScreen::kDefaultScreenBounds.width() / 2 -
                        kEditorMenuMinWidthDip - kEditorMenuMarginDip,

                    /*height=*/TestScreen::kDefaultScreenBounds.height() -
                        kEditorMenuHeightForTestDip * 2 -
                        kEditorMenuMarginDip * 2),
            },
        .editor_menu_bounds =
            {
                gfx::Point(
                    /*x=*/kEditorMenuMinWidthDip + kEditorMenuMarginDip,
                    /*y=*/0),
                gfx::Size(
                    /*width=*/kEditorMenuMinWidthDip,
                    /*height=*/kEditorMenuHeightForTestDip),
            },
    },
    // When:
    //  - Context menu leaves enough space on every side.
    //  - Cursor point is below context menu.
    // Then:
    //  - Editor menu appears below context menu.
    {
        .cursor_point = gfx::Point(
            /*x=*/TestScreen::kDefaultScreenBounds.width() / 2,
            /*y=*/TestScreen::kDefaultScreenBounds.height()),
        .anchor_view_bounds =
            {
                gfx::Point(
                    /*x=*/kEditorMenuMinWidthDip + kEditorMenuMarginDip,
                    /*y=*/kEditorMenuHeightForTestDip + kEditorMenuMarginDip),
                gfx::Size(
                    /*width=*/TestScreen::kDefaultScreenBounds.width() / 2 -
                        kEditorMenuMinWidthDip - kEditorMenuMarginDip,
                    /*height=*/TestScreen::kDefaultScreenBounds.height() -
                        kEditorMenuHeightForTestDip * 2 -
                        kEditorMenuMarginDip * 2),
            },
        .editor_menu_bounds =
            {
                gfx::Point(
                    /*x=*/kEditorMenuMinWidthDip + kEditorMenuMarginDip,
                    /*y=*/TestScreen::kDefaultScreenBounds.height() -
                        kEditorMenuHeightForTestDip),
                gfx::Size(
                    /*width=*/kEditorMenuMinWidthDip,
                    /*height=*/kEditorMenuHeightForTestDip),
            },
    },
    // When:
    //  - Context menu covers left half of the screen.
    //  - Cursor point is on top left corner of the screen.
    // Then:
    //  - Editor menu appears top right side of context menu.
    //  - Editor menu aligns top edge with context menu.
    {
        .cursor_point = gfx::Point(
            /*x=*/0,
            /*y=*/0),
        .anchor_view_bounds =
            {
                gfx::Point(
                    /*x=*/0,
                    /*y=*/0),
                gfx::Size(
                    /*width=*/TestScreen::kDefaultScreenBounds.width() / 2,
                    /*height=*/TestScreen::kDefaultScreenBounds.height()),
            },
        .editor_menu_bounds =
            {gfx::Point(
                 /*x=*/TestScreen::kDefaultScreenBounds.width() / 2 +
                     kEditorMenuMarginDip,
                 /*y=*/0),
             gfx::Size(
                 /*width=*/kEditorMenuMinWidthDip,
                 /*height=*/kEditorMenuHeightForTestDip)},
    },
    // When:
    //  - Context menu covers left half of the screen.
    //  - Cursor point is on bottom left corner of the screen.
    // Then:
    //  - Editor menu appears bottom right side of context menu.
    //  - Editor menu aligns bottom edge with context menu.
    {
        .cursor_point = gfx::Point(
            /*x=*/0,
            /*y=*/TestScreen::kDefaultScreenBounds.height()),
        .anchor_view_bounds =
            {
                gfx::Point(
                    /*x=*/0,
                    /*y=*/0),
                gfx::Size(
                    /*width=*/TestScreen::kDefaultScreenBounds.width() / 2,
                    /*height=*/TestScreen::kDefaultScreenBounds.height()),
            },
        .editor_menu_bounds =
            {gfx::Point(
                 /*x=*/TestScreen::kDefaultScreenBounds.width() / 2 +
                     kEditorMenuMarginDip,
                 /*y=*/TestScreen::kDefaultScreenBounds.height() -
                     kEditorMenuHeightForTestDip),
             gfx::Size(
                 /*width=*/kEditorMenuMinWidthDip,
                 /*height=*/kEditorMenuHeightForTestDip)},
    },
    // When:
    //  - Context menu covers right half of the screen.
    //  - Cursor point is on top right corner of the screen.
    // Then:
    //  - Editor menu appears top left side of context menu.
    //  - Editor menu aligns top edge with context menu.
    {
        .cursor_point = gfx::Point(
            /*x=*/TestScreen::kDefaultScreenBounds.width(),
            /*y=*/0),
        .anchor_view_bounds =
            {
                gfx::Point(
                    /*x=*/TestScreen::kDefaultScreenBounds.width() / 2,
                    /*y=*/0),
                gfx::Size(
                    /*width=*/TestScreen::kDefaultScreenBounds.width() / 2,
                    /*height=*/TestScreen::kDefaultScreenBounds.height()),
            },
        .editor_menu_bounds =
            {gfx::Point(
                 /*x=*/TestScreen::kDefaultScreenBounds.width() / 2 -
                     kEditorMenuMarginDip - kEditorMenuMinWidthDip,
                 /*y=*/0),
             gfx::Size(
                 /*width=*/kEditorMenuMinWidthDip,
                 /*height=*/kEditorMenuHeightForTestDip)},
    },
    // When:
    //  - Context menu covers right half of the screen.
    //  - Cursor point is top bottom right corner of the screen.
    // Then:
    //  - Editor menu appears bottom left side of context menu.
    //  - Editor menu aligns bottom edge with context menu.
    {
        .cursor_point = gfx::Point(
            /*x=*/TestScreen::kDefaultScreenBounds.width(),
            /*y=*/TestScreen::kDefaultScreenBounds.height()),
        .anchor_view_bounds =
            {
                gfx::Point(
                    /*x=*/TestScreen::kDefaultScreenBounds.width() / 2,
                    /*y=*/0),
                gfx::Size(
                    /*width=*/TestScreen::kDefaultScreenBounds.width() / 2,
                    /*height=*/TestScreen::kDefaultScreenBounds.height()),
            },
        .editor_menu_bounds =
            {gfx::Point(
                 /*x=*/TestScreen::kDefaultScreenBounds.width() / 2 -
                     kEditorMenuMarginDip - kEditorMenuMinWidthDip,
                 /*y=*/TestScreen::kDefaultScreenBounds.height() -
                     kEditorMenuHeightForTestDip),
             gfx::Size(
                 /*width=*/kEditorMenuMinWidthDip,
                 /*height=*/kEditorMenuHeightForTestDip)},
    },
};

class MockView : public views::View {
 public:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(kEditorMenuMinWidthDip, kEditorMenuHeightForTestDip);
  }
};

class GetEditorMenuBoundsTest
    : public TestWithParam<GetEditorMenuBoundsTestParams> {
 protected:
  std::unique_ptr<MockView> target_ = std::make_unique<MockView>();
  TestScreen test_screen_{/*create_display=*/true, /*register_screen=*/true};
};

TEST_P(GetEditorMenuBoundsTest, Verify) {
  test_screen_.set_cursor_screen_point(GetParam().cursor_point);

  const gfx::Rect editor_menu_bounds =
      chromeos::editor_menu::GetEditorMenuBounds(GetParam().anchor_view_bounds,
                                                 target_.get());

  EXPECT_EQ(editor_menu_bounds.x(), GetParam().editor_menu_bounds.x());
  EXPECT_EQ(editor_menu_bounds.y(), GetParam().editor_menu_bounds.y());
  EXPECT_EQ(editor_menu_bounds.width(), GetParam().editor_menu_bounds.width());
  EXPECT_EQ(editor_menu_bounds.height(),
            GetParam().editor_menu_bounds.height());
}

INSTANTIATE_TEST_SUITE_P(GetEditorMenuBoundsTestAll,
                         GetEditorMenuBoundsTest,
                         ValuesIn(get_editor_menu_bounds_test_cases));

}  // namespace

}  // namespace chromeos::editor_menu
