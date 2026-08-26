// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu.h"

#include <memory>
#include <utility>

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_test_base.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_zoom_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

class ActionAppMenuTest : public ActionAppMenuTestBase {
 public:
  ActionAppMenuTest() = default;
  ~ActionAppMenuTest() override = default;

  void SetUp() override {
    ActionAppMenuTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    button_ = widget_->SetContentsView(std::make_unique<views::MenuButton>(
        views::Button::PressedCallback(), u"Menu"));
  }

  void TearDown() override {
    button_ = nullptr;
    widget_.reset();
    ActionAppMenuTestBase::TearDown();
  }

 protected:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::MenuButton> button_ = nullptr;
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

TEST_F(ActionAppMenuTest, PopulatesSectionCardsWithStyling) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  // Check if the menu contains section menu items.
  views::MenuItemView* password_item =
      root->GetMenuItemByID(kActionPasswordsAndAutofillSubmenu);
  ASSERT_TRUE(password_item);

  views::MenuItemView* print_item = root->GetMenuItemByID(kActionPrint);
  ASSERT_TRUE(print_item);

  // Check if the styling is applied to the menu items.
  EXPECT_TRUE(password_item->GetMenuItemBackground().has_value());
  EXPECT_TRUE(print_item->GetMenuItemBackground().has_value());

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, PopulatesRecentTabsSubmenu) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* recent_tabs_item =
      root->GetMenuItemByID(kActionRecentTabsSubmenu);
  ASSERT_TRUE(recent_tabs_item);
  EXPECT_TRUE(recent_tabs_item->HasSubmenu());

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, PopulatesStaticSubmenus) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  // Passwords and autofill submenu.
  views::MenuItemView* password_item =
      root->GetMenuItemByID(kActionPasswordsAndAutofillSubmenu);
  ASSERT_TRUE(password_item);
  EXPECT_TRUE(password_item->HasSubmenu());
  EXPECT_NE(root->GetMenuItemByID(kActionShowPasswordManager), nullptr);
  EXPECT_NE(root->GetMenuItemByID(kActionShowPaymentMethods), nullptr);

  // Extensions submenu.
  views::MenuItemView* extensions_item =
      root->GetMenuItemByID(kActionExtensionsSubmenu);
  ASSERT_TRUE(extensions_item);
  EXPECT_TRUE(extensions_item->HasSubmenu());
  EXPECT_NE(root->GetMenuItemByID(kActionExtensionsSubmenuManageExtensions),
            nullptr);
  EXPECT_NE(root->GetMenuItemByID(kActionExtensionsSubmenuVisitChromeWebStore),
            nullptr);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, PopulatesTextAndIconOverrides) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  ASSERT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  // Verify text override is applied (kActionNewIncognitoWindow).
  views::MenuItemView* incognito_item =
      root->GetMenuItemByID(kActionNewIncognitoWindow);
  ASSERT_TRUE(incognito_item);
  EXPECT_EQ(incognito_item->title(), l10n_util::GetStringUTF16(IDS_INCOGNITO));

  // Verify icon override is applied (kActionNewTab).
  views::MenuItemView* new_tab_item = root->GetMenuItemByID(kActionNewTab);
  ASSERT_TRUE(new_tab_item);
  EXPECT_FALSE(new_tab_item->GetIcon().IsEmpty());

  // Verify an item without text override inherits delegate title.
  views::MenuItemView* window_item = root->GetMenuItemByID(kActionNewWindow);
  ASSERT_TRUE(window_item);
  EXPECT_EQ(window_item->title(), u"New Window");

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, ZoomMenuRowCreationAndChildren) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());
  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* zoom_item = root->GetMenuItemByID(kActionZoomSubmenu);
  ASSERT_TRUE(zoom_item);

  // The custom menu item should have a single child view which is
  // ActionAppMenuZoomView.
  ASSERT_EQ(zoom_item->children().size(), 1u);
  auto* zoom_view =
      views::AsViewClass<ActionAppMenuZoomView>(zoom_item->children()[0]);
  ASSERT_TRUE(zoom_view);

  // ActionAppMenuZoomView should contain the zoom minus button, zoom label,
  // zoom plus button, separator, and fullscreen button.
  EXPECT_GE(zoom_view->children().size(), 4u);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, ZoomChildActionsInvocation) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());
  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  actions::ActionItem* minus_action =
      actions::ActionManager::Get().FindAction(kActionZoomMinus);
  ASSERT_TRUE(minus_action);

  EXPECT_CALL(mock_action_invoked_,
              Call(kActionZoomMinus, testing::_, testing::_))
      .Times(1);
  minus_action->InvokeAction();
  testing::Mock::VerifyAndClearExpectations(&mock_action_invoked_);

  actions::ActionItem* plus_action =
      actions::ActionManager::Get().FindAction(kActionZoomPlus);
  ASSERT_TRUE(plus_action);

  EXPECT_CALL(mock_action_invoked_,
              Call(kActionZoomPlus, testing::_, testing::_))
      .Times(1);
  plus_action->InvokeAction();
  testing::Mock::VerifyAndClearExpectations(&mock_action_invoked_);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

}  // namespace
