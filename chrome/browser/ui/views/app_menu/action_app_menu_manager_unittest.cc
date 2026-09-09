// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"

#include "base/test/scoped_command_line.h"
#include "build/build_config.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_test_base.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using ActionAppMenuManagerTest = ActionAppMenuTestBase;

TEST_F(ActionAppMenuManagerTest, ProxySyncsWithDelegateAndInvokes) {
  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* delegate =
      actions::ActionManager::Get().FindAction(kActionShowPasswordManager);
  ASSERT_NE(delegate, nullptr);

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);
  actions::ActionItem* your_chrome_section =
      root->GetChildren().children()[1]->GetActionItem();
  ASSERT_NE(your_chrome_section, nullptr);
  actions::BaseAction* passwords_submenu = nullptr;
  for (const auto& child : your_chrome_section->GetChildren().children()) {
    if (child->GetActionItem()->GetActionId() ==
        kActionPasswordsAndAutofillSubmenu) {
      passwords_submenu = child.get();
      break;
    }
  }
  ASSERT_NE(passwords_submenu, nullptr);
  actions::ActionItem* passwords_proxy =
      passwords_submenu->GetChildren().children()[0]->GetActionItem();

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

TEST_F(ActionAppMenuManagerTest, BlockActionsGuestSessionExcludesIncognito) {
  profile_->SetGuestSession(true);

  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);

  // The block section is at index 0.
  actions::ActionItem* block_section =
      root->GetChildren().children()[0]->GetActionItem();
  ASSERT_NE(block_section, nullptr);

  // Only New Tab and New Window should be present for a guest session.
  EXPECT_EQ(block_section->GetChildren().children().size(), 2u);
}

// Profile submenu is disabled for ChromeOS
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ProfileSubmenu DISABLED_ProfileSubmenu
#else
#define MAYBE_ProfileSubmenu ProfileSubmenu
#endif
TEST_F(ActionAppMenuManagerTest, MAYBE_ProfileSubmenu) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  signin::MakePrimaryAccountAvailable(identity_manager, "test@example.com",
                                      signin::ConsentLevel::kSignin);

  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);

  actions::ActionItem* your_chrome_section =
      root->GetChildren().children()[1]->GetActionItem();
  ASSERT_NE(your_chrome_section, nullptr);

  ASSERT_GE(your_chrome_section->GetChildren().children().size(), 1u);
  actions::BaseAction* profile_submenu =
      your_chrome_section->GetChildren().children()[0].get();
  ASSERT_NE(profile_submenu, nullptr);
  EXPECT_EQ(profile_submenu->GetActionItem()->GetActionId(),
            kActionProfileSubmenu);
  EXPECT_EQ(profile_submenu->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kRow);

  // It should contain a single child: kActionManageGoogleAccount.
  ASSERT_EQ(profile_submenu->GetChildren().children().size(), 1u);
  actions::BaseAction* manage_google_account =
      profile_submenu->GetChildren().children()[0].get();
  ASSERT_NE(manage_google_account, nullptr);
  EXPECT_EQ(manage_google_account->GetActionItem()->GetActionId(),
            kActionManageGoogleAccount);
  EXPECT_EQ(manage_google_account->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kRow);
}

TEST_F(ActionAppMenuManagerTest,
       BlockActionsEnterpriseIsolatedModeReplacesIncognito) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      enterprise_isolated_mode::switches::
          kForceEnterpriseIsolatedModeReplacesIncognito);

  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);

  actions::ActionItem* block_section =
      root->GetChildren().children()[0]->GetActionItem();
  ASSERT_NE(block_section, nullptr);

  // When isolated mode replaces incognito, it should replace the New Incognito
  // Window action rather than adding a fourth item.
  ASSERT_EQ(block_section->GetChildren().children().size(), 3u);
  EXPECT_EQ(block_section->GetChildren()
                .children()[0]
                ->GetActionItem()
                ->GetActionId(),
            kActionNewTab);
  EXPECT_EQ(block_section->GetChildren()
                .children()[1]
                ->GetActionItem()
                ->GetActionId(),
            kActionNewWindow);
  EXPECT_EQ(block_section->GetChildren()
                .children()[2]
                ->GetActionItem()
                ->GetActionId(),
            kActionNewIsolatedWindow);
}

}  // namespace
