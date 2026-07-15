// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"

#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

class OmniboxEverywhereUIManagerTest : public ChromeViewsTestBase {
 public:
  OmniboxEverywhereUIManagerTest() = default;
  ~OmniboxEverywhereUIManagerTest() override = default;
};

TEST_F(OmniboxEverywhereUIManagerTest, ShowAndCloseWidget) {
  omnibox_everywhere::OmniboxEverywhereUIManager ui_manager;

  // Initially, no widget should exist.
  EXPECT_FALSE(ui_manager.widget_for_testing());

  // Showing the UI manager should instantiate and display a widget.
  ui_manager.Show(GetContext());
  views::Widget* widget = ui_manager.widget_for_testing();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Closing the UI manager should trigger widget closure.
  views::test::WidgetDestroyedWaiter waiter(widget);
  ui_manager.Close();
  waiter.Wait();

  EXPECT_FALSE(ui_manager.widget_for_testing());
}

TEST_F(OmniboxEverywhereUIManagerTest, ShowWhileWidgetIsClosing) {
  omnibox_everywhere::OmniboxEverywhereUIManager ui_manager;

  ui_manager.Show(GetContext());
  views::Widget* first_widget = ui_manager.widget_for_testing();
  ASSERT_TRUE(first_widget);

  // Close the widget. Because of MakeCloseSynchronous, this immediately
  // resets the manager's widget pointer.
  ui_manager.Close();
  EXPECT_FALSE(ui_manager.widget_for_testing());

  // Showing it again immediately should successfully create a new widget.
  ui_manager.Show(GetContext());
  views::Widget* second_widget = ui_manager.widget_for_testing();
  ASSERT_TRUE(second_widget);
  EXPECT_NE(first_widget, second_widget);
  EXPECT_TRUE(second_widget->IsVisible());

  // Clean up.
  views::test::WidgetDestroyedWaiter waiter(second_widget);
  ui_manager.Close();
  waiter.Wait();
  EXPECT_FALSE(ui_manager.widget_for_testing());
}
