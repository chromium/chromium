// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/views/app_menu/app_menu_action_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/widget/widget.h"

class ActionAppMenuTest : public ChromeViewsTestBase {
 public:
  ActionAppMenuTest() = default;
  ~ActionAppMenuTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    actions::ActionManager::Get().ResetActions();

    // Create test ActionItems as children of a root ActionItem.
    auto root = actions::ActionItem::Builder().Build();
    root->AddChild(actions::ActionItem::Builder(base::DoNothing())
                       .SetActionId(kActionShowPasswordManager)
                       .SetText(u"Password Manager")
                       .SetEnabled(true)
                       .SetVisible(true)
                       .Build());
    root->AddChild(actions::ActionItem::Builder(base::DoNothing())
                       .SetActionId(kActionPrint)
                       .SetText(u"Print")
                       .SetEnabled(true)
                       .SetVisible(true)
                       .Build());
    actions::ActionManager::Get().AddAction(std::move(root));

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    button_ = widget_->SetContentsView(std::make_unique<views::MenuButton>(
        views::Button::PressedCallback(), u"Menu"));
  }

  void TearDown() override {
    button_ = nullptr;
    widget_.reset();
    actions::ActionManager::Get().ResetActions();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::MenuButton> button_ = nullptr;
  testing::NiceMock<MockBrowserWindowInterface> mock_window_interface_;
};

TEST_F(ActionAppMenuTest, RunAndCloseMenu) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  auto action_manager = std::make_unique<AppMenuActionManager>();
  action_manager->Initialize();

  ActionAppMenu menu(&mock_window_interface_, std::move(action_manager),
                     on_menu_closed.Get());

  EXPECT_FALSE(menu.IsShowing());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
  EXPECT_FALSE(menu.IsShowing());
}

TEST_F(ActionAppMenuTest, PopulatesSectionCardsWithStyling) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  auto action_manager = std::make_unique<AppMenuActionManager>();
  action_manager->Initialize();

  ActionAppMenu menu(&mock_window_interface_, std::move(action_manager),
                     on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  // Check if the menu contains section menu items.
  views::MenuItemView* password_item =
      root->GetMenuItemByID(kActionShowPasswordManager);
  ASSERT_TRUE(password_item);

  views::MenuItemView* print_item = root->GetMenuItemByID(kActionPrint);
  ASSERT_TRUE(print_item);

  // Check if the styling is applied to the menu items.
  EXPECT_TRUE(password_item->GetMenuItemBackground().has_value());
  EXPECT_TRUE(print_item->GetMenuItemBackground().has_value());

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}
