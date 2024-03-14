// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"

#include <memory>

#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

namespace chromeos::mahi {

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

}  // namespace chromeos::mahi
