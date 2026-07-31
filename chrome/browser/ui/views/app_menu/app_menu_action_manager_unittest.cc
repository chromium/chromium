// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_action_manager.h"

#include <memory>
#include <string>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/views/app_menu/app_menu_section_action_item.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::_;

class AppMenuActionManagerTest : public testing::Test {
 public:
  AppMenuActionManagerTest() = default;
  ~AppMenuActionManagerTest() override = default;

  void RegisterTestAction(actions::ActionItem* parent,
                          actions::ActionId action_id,
                          const std::u16string& text) {
    parent->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(&MockActionCallback::Call,
                                base::Unretained(&mock_action_invoked_),
                                action_id))
            .SetActionId(action_id)
            .SetText(text)
            .SetEnabled(true)
            .SetVisible(true)
            .Build());
  }

  void SetUp() override {
    testing::Test::SetUp();
    actions::ActionManager::Get().ResetActions();

    // Register all action items present in GetMenuHierarchy().
    auto root = actions::ActionItem::Builder().Build();
    RegisterTestAction(root.get(), kActionShowPasswordManager,
                       u"Passwords and autofill");
    RegisterTestAction(root.get(), kActionShowHistory, u"History");
    RegisterTestAction(root.get(), kActionManageExtensions, u"Extensions");
    RegisterTestAction(root.get(), kActionPrint, u"Print");
    RegisterTestAction(root.get(), kActionFind, u"Find and Edit");

    actions::ActionManager::Get().AddAction(std::move(root));
  }

  void TearDown() override {
    actions::ActionManager::Get().ResetActions();
    testing::Test::TearDown();
  }

 protected:
  using MockActionCallback =
      testing::MockFunction<void(actions::ActionId,
                                 actions::ActionItem*,
                                 actions::ActionInvocationContext)>;
  MockActionCallback mock_action_invoked_;
};

TEST_F(AppMenuActionManagerTest, InitializeInflatesHierarchy) {
  AppMenuActionManager manager;
  manager.Initialize();

  actions::ActionItem* root = manager.root_action_item();
  ASSERT_NE(root, nullptr);
  ASSERT_EQ(root->GetChildren().children().size(), 2u);

  actions::ActionItem* your_chrome_section =
      root->GetChildren().children()[0]->GetActionItem();
  EXPECT_TRUE(
      actions::IsActionClass<AppMenuSectionActionItem>(your_chrome_section));
  EXPECT_EQ(your_chrome_section->GetText(),
            l10n_util::GetStringUTF16(IDS_APP_MENU_YOUR_CHROME_HEADER));
  EXPECT_EQ(your_chrome_section->GetProperty(
                AppMenuActionManager::kAppMenuDisplayTypeKey),
            MenuEntry::DisplayType::kRow);

  const auto& your_chrome_children =
      your_chrome_section->GetChildren().children();
  ASSERT_EQ(your_chrome_children.size(), 3u);

  // Bug(crbug.com/540467430): Commented out temporarily in order to avoid
  // compilation errors for action items.

  // actions::ActionItem* passwords_proxy = your_chrome_children[0].get();
  // EXPECT_TRUE(
  //     actions::IsActionItemClass<AppMenuProxyActionItem>(passwords_proxy));
  // EXPECT_EQ(passwords_proxy->GetActionId(), kActionShowPasswordManager);
  // EXPECT_EQ(passwords_proxy->GetText(), u"Passwords and autofill");

  // actions::ActionItem* tools_and_actions_section =
  //     root->GetChildren().children()[1].get();
  // EXPECT_TRUE(actions::IsActionItemClass<AppMenuSectionActionItem>(
  //     tools_and_actions_section));
  // EXPECT_EQ(tools_and_actions_section->GetText(),
  //           l10n_util::GetStringUTF16(IDS_APP_MENU_TOOLS_AND_ACTIONS_HEADER));

  // const auto& tools_and_actions_children =
  //     tools_and_actions_section->GetChildren().children();
  // ASSERT_EQ(tools_and_actions_children.size(), 2u);

  // actions::ActionItem* print_proxy = tools_and_actions_children[0].get();
  // EXPECT_TRUE(actions::IsActionItemClass<AppMenuProxyActionItem>(print_proxy));
  // EXPECT_EQ(print_proxy->GetActionId(), kActionPrint);
  // EXPECT_EQ(print_proxy->GetText(), u"Print");
}

TEST_F(AppMenuActionManagerTest, ProxySyncsWithDelegateAndInvokes) {
  AppMenuActionManager manager;
  manager.Initialize();

  actions::ActionItem* delegate =
      actions::ActionManager::Get().FindAction(kActionShowPasswordManager);
  ASSERT_NE(delegate, nullptr);

  actions::ActionItem* root = manager.root_action_item();
  actions::ActionItem* passwords_proxy = root->GetChildren()
                                             .children()[0]
                                             ->GetChildren()
                                             .children()[0]
                                             ->GetActionItem();

  // Test dynamic synchronization.
  EXPECT_EQ(passwords_proxy->GetText(), u"Passwords and autofill");
  delegate->SetText(u"Passwords Title");
  EXPECT_EQ(passwords_proxy->GetText(), u"Passwords Title");

  EXPECT_TRUE(passwords_proxy->GetEnabled());
  delegate->SetEnabled(false);
  EXPECT_FALSE(passwords_proxy->GetEnabled());

  // Re-enable the delegate so that InvokeAction() can execute the callback.
  delegate->SetEnabled(true);
  EXPECT_TRUE(passwords_proxy->GetEnabled());

  EXPECT_CALL(mock_action_invoked_, Call(kActionShowPasswordManager, _, _))
      .Times(1);
  passwords_proxy->InvokeAction();
  testing::Mock::VerifyAndClearExpectations(&mock_action_invoked_);
}
