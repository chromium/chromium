// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/action_app_menu.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/widget/widget.h"

class ActionAppMenuTest : public ChromeViewsTestBase {
 public:
  ActionAppMenuTest() = default;
  ~ActionAppMenuTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    button_ = widget_->SetContentsView(std::make_unique<views::MenuButton>(
        views::Button::PressedCallback(), u"Menu"));
  }

  void TearDown() override {
    button_ = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::MenuButton> button_ = nullptr;
  testing::NiceMock<MockBrowserWindowInterface> mock_window_interface_;
};

TEST_F(ActionAppMenuTest, RunAndCloseMenu) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  EXPECT_FALSE(menu.IsShowing());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
  EXPECT_FALSE(menu.IsShowing());
}
