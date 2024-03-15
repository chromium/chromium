// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"

#include <memory>

#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace chromeos::mahi {

namespace {

constexpr int kQuickAnswersAndMahiSpacing = 10;
constexpr int kDefaultWidth = 100;

std::unique_ptr<views::View> CreateViewWithHeight(int height) {
  std::unique_ptr<views::View> view = std::make_unique<views::View>();
  view->SetPreferredSize(gfx::Size(kDefaultWidth, height));
  return view;
}

}  // namespace

using ReadWriteCardsUiControllerTest = ChromeViewsTestBase;

TEST_F(ReadWriteCardsUiControllerTest, SetQuickAnswersView) {
  ReadWriteCardsUiController controller;
  EXPECT_FALSE(controller.widget_for_test());

  views::View* test_view =
      controller.SetQuickAnswersView(std::make_unique<views::View>());

  EXPECT_TRUE(controller.widget_for_test());
  EXPECT_TRUE(controller.widget_for_test()->IsVisible());
  EXPECT_EQ(test_view, controller.GetQuickAnswersViewForTest());

  controller.RemoveQuickAnswersView();
  EXPECT_FALSE(controller.widget_for_test());
  EXPECT_FALSE(controller.GetQuickAnswersViewForTest());
}

TEST_F(ReadWriteCardsUiControllerTest, SetMahiView) {
  ReadWriteCardsUiController controller;
  EXPECT_FALSE(controller.widget_for_test());

  views::View* test_view =
      controller.SetMahiView(std::make_unique<views::View>());

  EXPECT_TRUE(controller.widget_for_test());
  EXPECT_TRUE(controller.widget_for_test()->IsVisible());
  EXPECT_EQ(test_view, controller.GetMahiViewForTest());

  controller.RemoveMahiView();
  EXPECT_FALSE(controller.widget_for_test());
  EXPECT_FALSE(controller.GetMahiViewForTest());
}

TEST_F(ReadWriteCardsUiControllerTest, SetQuickAnswersAndMahiView) {
  ReadWriteCardsUiController controller;
  EXPECT_FALSE(controller.widget_for_test());

  views::View* test_quick_answers_view =
      controller.SetQuickAnswersView(std::make_unique<views::View>());

  views::View* test_mahi_view =
      controller.SetMahiView(std::make_unique<views::View>());

  EXPECT_TRUE(controller.widget_for_test());
  EXPECT_TRUE(controller.widget_for_test()->IsVisible());
  EXPECT_EQ(test_quick_answers_view, controller.GetQuickAnswersViewForTest());
  EXPECT_EQ(test_mahi_view, controller.GetMahiViewForTest());

  controller.RemoveQuickAnswersView();

  // The widget should still show since mahi view is still visible.
  EXPECT_TRUE(controller.widget_for_test());
  EXPECT_TRUE(controller.widget_for_test()->IsVisible());
  EXPECT_FALSE(controller.GetQuickAnswersViewForTest());
  EXPECT_EQ(test_mahi_view, controller.GetMahiViewForTest());

  test_quick_answers_view =
      controller.SetQuickAnswersView(std::make_unique<views::View>());

  EXPECT_TRUE(controller.widget_for_test());
  EXPECT_TRUE(controller.widget_for_test()->IsVisible());
  EXPECT_EQ(test_quick_answers_view, controller.GetQuickAnswersViewForTest());
  EXPECT_EQ(test_mahi_view, controller.GetMahiViewForTest());

  controller.RemoveMahiView();

  // The widget should still show since quick answers view is still visible.
  EXPECT_TRUE(controller.widget_for_test());
  EXPECT_TRUE(controller.widget_for_test()->IsVisible());
  EXPECT_EQ(test_quick_answers_view, controller.GetQuickAnswersViewForTest());
  EXPECT_FALSE(controller.GetMahiViewForTest());

  controller.RemoveQuickAnswersView();

  EXPECT_FALSE(controller.widget_for_test());
  EXPECT_FALSE(controller.GetQuickAnswersViewForTest());
}

TEST_F(ReadWriteCardsUiControllerTest, WidgetBoundsDefault) {
  ReadWriteCardsUiController controller;

  gfx::Rect context_menu_bounds =
      gfx::Rect(gfx::Point(500, 250), gfx::Size(kDefaultWidth, 140));
  controller.SetContextMenuBounds(context_menu_bounds);

  int view_height = 80;
  controller.SetMahiView(CreateViewWithHeight(view_height));
  ASSERT_TRUE(controller.widget_for_test());
  gfx::Rect widget_bounds = controller.widget_for_test()->GetRestoredBounds();

  // Widget bounds should vertically aligned with context menu.
  EXPECT_EQ(widget_bounds.x(), context_menu_bounds.x());
  EXPECT_EQ(widget_bounds.right(), context_menu_bounds.right());

  // Widget is positioned above context menu.
  EXPECT_EQ(widget_bounds.bottom() + kQuickAnswersAndMahiSpacing,
            context_menu_bounds.y());

  EXPECT_EQ(view_height, widget_bounds.height());
  EXPECT_EQ(kDefaultWidth, widget_bounds.width());
}

TEST_F(ReadWriteCardsUiControllerTest, WidgetBoundsBelowContextMenu) {
  ReadWriteCardsUiController controller;

  gfx::Rect context_menu_bounds =
      gfx::Rect(gfx::Point(500, 250), gfx::Size(kDefaultWidth, 140));
  // Update context menu's position so that it does not leave enough vertical
  // space above it to show the widget.
  context_menu_bounds.set_y(10);
  controller.SetContextMenuBounds(context_menu_bounds);

  int view_height = 80;
  controller.SetQuickAnswersView(CreateViewWithHeight(view_height));
  ASSERT_TRUE(controller.widget_for_test());
  gfx::Rect widget_bounds = controller.widget_for_test()->GetRestoredBounds();

  // Widget bounds should vertically aligned with context menu.
  EXPECT_EQ(widget_bounds.x(), context_menu_bounds.x());
  EXPECT_EQ(widget_bounds.right(), context_menu_bounds.right());

  // Context menu is positioned above the view.
  EXPECT_EQ(context_menu_bounds.bottom() + kQuickAnswersAndMahiSpacing,
            widget_bounds.y());

  EXPECT_EQ(view_height, widget_bounds.height());
  EXPECT_EQ(kDefaultWidth, widget_bounds.width());
}

TEST_F(ReadWriteCardsUiControllerTest, WidgetBoundsForBoth) {
  ReadWriteCardsUiController controller;

  gfx::Rect context_menu_bounds =
      gfx::Rect(gfx::Point(500, 250), gfx::Size(kDefaultWidth, 140));
  controller.SetContextMenuBounds(context_menu_bounds);

  int mahi_height = 80;
  int qa_height = 90;
  controller.SetMahiView(CreateViewWithHeight(mahi_height));
  controller.SetQuickAnswersView(CreateViewWithHeight(qa_height));
  ASSERT_TRUE(controller.widget_for_test());
  gfx::Rect widget_bounds = controller.widget_for_test()->GetRestoredBounds();

  // Widget is positioned above context menu.
  EXPECT_EQ(widget_bounds.bottom() + kQuickAnswersAndMahiSpacing,
            context_menu_bounds.y());

  EXPECT_EQ(mahi_height + qa_height + kQuickAnswersAndMahiSpacing,
            widget_bounds.height());

  controller.RemoveQuickAnswersView();
  widget_bounds = controller.widget_for_test()->GetRestoredBounds();

  // Widget is still positioned above context menu.
  EXPECT_EQ(widget_bounds.bottom() + kQuickAnswersAndMahiSpacing,
            context_menu_bounds.y());

  EXPECT_EQ(mahi_height, widget_bounds.height());
}

}  // namespace chromeos::mahi
