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
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/views/app_menu/app_menu_action_helper.h"
#include "chrome/test/base/testing_profile.h"
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
    profile_ = std::make_unique<TestingProfile>();
    ON_CALL(mock_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));

    actions::ActionManager::Get().ResetActions();

    // Create test ActionItems as children of a root ActionItem.
    auto root = actions::ActionItem::Builder().Build();
    root->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(&MockActionCallback::Call,
                                base::Unretained(&mock_action_invoked_),
                                kActionShowPasswordManager))
            .SetActionId(kActionShowPasswordManager)
            .SetText(u"Password Manager")
            .SetEnabled(true)
            .SetVisible(true)
            .Build());
    root->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(&MockActionCallback::Call,
                                base::Unretained(&mock_action_invoked_),
                                kActionShowHistory))
            .SetActionId(kActionShowHistory)
            .SetText(u"History")
            .SetEnabled(true)
            .SetVisible(true)
            .Build());
    root->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(&MockActionCallback::Call,
                                base::Unretained(&mock_action_invoked_),
                                kActionManageExtensions))
            .SetActionId(kActionManageExtensions)
            .SetText(u"Extensions")
            .SetEnabled(true)
            .SetVisible(true)
            .Build());
    root->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(&MockActionCallback::Call,
                                base::Unretained(&mock_action_invoked_),
                                kActionPrint))
            .SetActionId(kActionPrint)
            .SetText(u"Print")
            .SetEnabled(true)
            .SetVisible(true)
            .Build());
    root->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(&MockActionCallback::Call,
                                base::Unretained(&mock_action_invoked_),
                                kActionFind))
            .SetActionId(kActionFind)
            .SetText(u"Find")
            .SetEnabled(true)
            .SetVisible(true)
            .Build());
    actions::ActionManager::Get().AddAction(std::move(root));

    auto app_menu_root =
        actions::ActionItem::Builder().SetActionId(kActionAppMenuRoot).Build();
    root_action_ =
        actions::ActionManager::Get().AddAction(std::move(app_menu_root));
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    button_ = widget_->SetContentsView(std::make_unique<views::MenuButton>(
        views::Button::PressedCallback(), u"Menu"));
    browser_actions_ =
        std::make_unique<BrowserActions>(&mock_window_interface_);
  }

  void TearDown() override {
    root_action_ = nullptr;
    browser_actions_.reset();
    button_ = nullptr;
    widget_.reset();
    profile_.reset();
    actions::ActionManager::Get().ResetActions();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::MenuButton> button_ = nullptr;
  testing::NiceMock<MockBrowserWindowInterface> mock_window_interface_;
  std::unique_ptr<BrowserActions> browser_actions_;
  raw_ptr<actions::ActionItem> root_action_ = nullptr;

  using MockActionCallback =
      testing::MockFunction<void(actions::ActionId,
                                 actions::ActionItem*,
                                 actions::ActionInvocationContext)>;
  MockActionCallback mock_action_invoked_;
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

TEST_F(ActionAppMenuTest, ProxySyncsWithDelegateAndInvokes) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  actions::ActionItem* delegate =
      actions::ActionManager::Get().FindAction(kActionShowPasswordManager);
  ASSERT_NE(delegate, nullptr);

  actions::ActionItem* root = app_menu::GetAppMenuRoot(&mock_window_interface_);
  actions::ActionItem* passwords_proxy = root->GetChildren()
                                             .children()[0]
                                             ->GetChildren()
                                             .children()[0]
                                             ->GetActionItem();

  // Test dynamic synchronization.
  EXPECT_EQ(passwords_proxy->GetText(), u"Password Manager");
  delegate->SetText(u"Passwords Title");
  EXPECT_EQ(passwords_proxy->GetText(), u"Passwords Title");

  EXPECT_TRUE(passwords_proxy->GetEnabled());
  delegate->SetEnabled(false);
  EXPECT_FALSE(passwords_proxy->GetEnabled());

  // Re-enable the delegate so that InvokeAction() can execute the callback.
  delegate->SetEnabled(true);
  EXPECT_TRUE(passwords_proxy->GetEnabled());

  EXPECT_CALL(mock_action_invoked_,
              Call(kActionShowPasswordManager, testing::_, testing::_))
      .Times(1);
  passwords_proxy->InvokeAction();
  testing::Mock::VerifyAndClearExpectations(&mock_action_invoked_);
}
