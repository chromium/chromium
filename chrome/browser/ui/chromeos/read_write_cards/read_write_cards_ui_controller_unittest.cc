// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace chromeos::mahi {

namespace {

constexpr int kQuickAnswersAndMahiSpacing = 10;
constexpr int kDefaultWidth = 100;

class TestReadWriteCardsView : public ReadWriteCardsView {
  METADATA_HEADER(TestReadWriteCardsView, ReadWriteCardsView)

 public:
  explicit TestReadWriteCardsView(ReadWriteCardsUiController& controller,
                                  int maximum_height = 0)
      : ReadWriteCardsView(controller), maximum_height_(maximum_height) {}

  TestReadWriteCardsView(const TestReadWriteCardsView&) = delete;
  TestReadWriteCardsView& operator=(const TestReadWriteCardsView&) = delete;

  ~TestReadWriteCardsView() override = default;

  // ReadWriteCardsView:
  void UpdateBoundsForQuickAnswers() override { update_bounds_called_++; }
  gfx::Size GetMaximumSize() const override {
    return gfx::Size(0, maximum_height_);
  }

  int update_bounds_called() { return update_bounds_called_; }

 private:
  int update_bounds_called_ = 0;
  size_t maximum_height_ = 0;
};

std::unique_ptr<TestReadWriteCardsView> CreateViewWithHeight(
    ReadWriteCardsUiController& controller,
    int height,
    int maximum_height = 0) {
  std::unique_ptr<TestReadWriteCardsView> view =
      std::make_unique<TestReadWriteCardsView>(controller, maximum_height);
  view->SetPreferredSize(gfx::Size(kDefaultWidth, height));
  return view;
}

BEGIN_METADATA(TestReadWriteCardsView)
END_METADATA

}  // namespace

class ReadWriteCardsUiControllerTest
    : public ChromeViewsTestBase,
      public testing::WithParamInterface<bool> {
 public:
  ReadWriteCardsUiControllerTest() {
    scoped_feature_list_.InitWithFeatureState(chromeos::features::kMahi,
                                              IsMahiEnabled());
  }
  ReadWriteCardsUiControllerTest(const ReadWriteCardsUiControllerTest&) =
      delete;
  ReadWriteCardsUiControllerTest& operator=(
      const ReadWriteCardsUiControllerTest&) = delete;
  ~ReadWriteCardsUiControllerTest() override = default;

  bool IsMahiEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ReadWriteCardsUiControllerTest,
                         /*IsMahiEnabled()=*/testing::Bool());

TEST_P(ReadWriteCardsUiControllerTest, SetQuickAnswersView) {
  ReadWriteCardsUiController controller;

  gfx::Rect context_menu_bounds =
      gfx::Rect(gfx::Point(500, 250), gfx::Size(kDefaultWidth, 140));
  controller.SetContextMenuBounds(context_menu_bounds);

  ASSERT_FALSE(controller.widget_for_test());

  views::View* test_view = controller.SetQuickAnswersView(
      std::make_unique<TestReadWriteCardsView>(controller));

  EXPECT_TRUE(controller.widget_for_test());
  EXPECT_TRUE(controller.widget_for_test()->IsVisible());

  auto* qa_view = controller.GetQuickAnswersViewForTest();
  EXPECT_EQ(test_view, qa_view);
  EXPECT_EQ(context_menu_bounds, qa_view->context_menu_bounds_for_test());

  controller.RemoveQuickAnswersView();
  EXPECT_FALSE(controller.widget_for_test());
  EXPECT_FALSE(controller.GetQuickAnswersViewForTest());
}

TEST_P(ReadWriteCardsUiControllerTest, SetMahiView) {
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

TEST_P(ReadWriteCardsUiControllerTest, SetQuickAnswersAndMahiView) {
  ReadWriteCardsUiController controller;
  EXPECT_FALSE(controller.widget_for_test());

  views::View* test_quick_answers_view = controller.SetQuickAnswersView(
      std::make_unique<TestReadWriteCardsView>(controller));

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

  test_quick_answers_view = controller.SetQuickAnswersView(
      std::make_unique<TestReadWriteCardsView>(controller));

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

TEST_P(ReadWriteCardsUiControllerTest, ViewUpdateBounds) {
  ReadWriteCardsUiController controller;
  EXPECT_FALSE(controller.widget_for_test());

  ReadWriteCardsView* test_view = controller.SetQuickAnswersView(
      std::make_unique<TestReadWriteCardsView>(controller));
  TestReadWriteCardsView* read_write_cards_view =
      views::AsViewClass<TestReadWriteCardsView>(test_view);

  // Update bounds is called when the view is added to a widget.
  EXPECT_EQ(1, read_write_cards_view->update_bounds_called());

  controller.SetContextMenuBounds(
      gfx::Rect(gfx::Point(500, 250), gfx::Size(kDefaultWidth, 140)));
  EXPECT_EQ(2, read_write_cards_view->update_bounds_called());
}

TEST_P(ReadWriteCardsUiControllerTest, WidgetBoundsDefault) {
  ReadWriteCardsUiController controller;

  gfx::Rect context_menu_bounds =
      gfx::Rect(gfx::Point(500, 250), gfx::Size(kDefaultWidth, 140));
  controller.SetContextMenuBounds(context_menu_bounds);

  int view_height = 80;
  controller.SetMahiView(CreateViewWithHeight(controller, view_height));
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

TEST_P(ReadWriteCardsUiControllerTest, WidgetBoundsBelowContextMenu) {
  ReadWriteCardsUiController controller;

  gfx::Rect context_menu_bounds =
      gfx::Rect(gfx::Point(500, 250), gfx::Size(kDefaultWidth, 140));
  // Update context menu's position so that it does not leave enough vertical
  // space above it to show the widget.
  context_menu_bounds.set_y(10);
  controller.SetContextMenuBounds(context_menu_bounds);

  int view_height = 80;
  controller.SetQuickAnswersView(CreateViewWithHeight(controller, view_height));
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

TEST_P(ReadWriteCardsUiControllerTest, WidgetBoundsForBoth) {
  ReadWriteCardsUiController controller;

  gfx::Rect context_menu_bounds =
      gfx::Rect(gfx::Point(500, 250), gfx::Size(kDefaultWidth, 140));
  controller.SetContextMenuBounds(context_menu_bounds);

  int mahi_height = 80;
  int qa_height = 90;
  controller.SetMahiView(CreateViewWithHeight(controller, mahi_height));
  controller.SetQuickAnswersView(CreateViewWithHeight(controller, qa_height));
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

  controller.RemoveMahiView();
  EXPECT_FALSE(controller.widget_for_test());
}

TEST_P(ReadWriteCardsUiControllerTest, WidgetBoundsWithExtraReservedHeight) {
  ReadWriteCardsUiController controller;

  gfx::Rect context_menu_bounds =
      gfx::Rect(gfx::Point(500, 250), gfx::Size(kDefaultWidth, 140));
  context_menu_bounds.set_y(100);
  controller.SetContextMenuBounds(context_menu_bounds);

  int view_height = 80;
  controller.SetQuickAnswersView(
      CreateViewWithHeight(controller, view_height, /*maximum_height=*/120));
  ASSERT_TRUE(controller.widget_for_test());
  gfx::Rect widget_bounds = controller.widget_for_test()->GetRestoredBounds();

  // Context menu should be positioned above the view, because the maximum
  // height exceeds available height.
  EXPECT_EQ(context_menu_bounds.bottom() + kQuickAnswersAndMahiSpacing,
            widget_bounds.y());

  EXPECT_EQ(view_height, widget_bounds.height());
  EXPECT_EQ(kDefaultWidth, widget_bounds.width());
}

TEST_P(ReadWriteCardsUiControllerTest, ChildViewsPosition) {
  ReadWriteCardsUiController controller;

  gfx::Rect context_menu_bounds =
      gfx::Rect(gfx::Point(500, 250), gfx::Size(kDefaultWidth, 140));
  controller.SetContextMenuBounds(context_menu_bounds);

  int mahi_height = 80;
  int qa_height = 90;
  auto* mahi_view =
      controller.SetMahiView(CreateViewWithHeight(controller, mahi_height));
  auto* qa_view = controller.SetQuickAnswersView(
      CreateViewWithHeight(controller, qa_height));
  auto* widget = controller.widget_for_test();
  ASSERT_TRUE(widget);
  gfx::Rect widget_bounds = widget->GetRestoredBounds();

  auto* contents_view = widget->GetContentsView();

  // Widget is positioned above context menu.
  EXPECT_EQ(widget_bounds.bottom() + kQuickAnswersAndMahiSpacing,
            context_menu_bounds.y());

  // Quick Answers view is above Mahi view.
  EXPECT_EQ(0u, contents_view->GetIndexOf(qa_view));
  EXPECT_EQ(1u, contents_view->GetIndexOf(mahi_view));

  context_menu_bounds.set_y(10);
  controller.SetContextMenuBounds(context_menu_bounds);
  widget_bounds = widget->GetRestoredBounds();

  // Context menu is positioned above the view.
  EXPECT_EQ(context_menu_bounds.bottom() + kQuickAnswersAndMahiSpacing,
            widget_bounds.y());

  // Mahi view is above Quick Answers view.
  EXPECT_EQ(0u, contents_view->GetIndexOf(mahi_view));
  EXPECT_EQ(1u, contents_view->GetIndexOf(qa_view));
}

}  // namespace chromeos::mahi
