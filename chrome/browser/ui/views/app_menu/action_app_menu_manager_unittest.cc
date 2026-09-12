// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"

#include "base/test/scoped_command_line.h"
#include "build/build_config.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/profiles/profile_view_utils.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_test_base.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/test_sync_service.h"
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
#define MAYBE_ProfileSubmenuSingleProfile DISABLED_ProfileSubmenuSingleProfile
#else
#define MAYBE_ProfileSubmenu ProfileSubmenu
#define MAYBE_ProfileSubmenuSingleProfile ProfileSubmenuSingleProfile
#endif
TEST_F(ActionAppMenuManagerTest, MAYBE_ProfileSubmenu) {
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  TestingProfile* profile1 = profile_manager.CreateTestingProfile("Profile 1");
  profile_manager.CreateTestingProfile("Profile 2");

  SyncServiceFactory::GetInstance()->SetTestingFactory(
      profile1,
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return std::make_unique<syncer::TestSyncService>();
          }));

  ON_CALL(mock_window_interface_, GetProfile())
      .WillByDefault(testing::Return(profile1));

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile1);
  signin::MakePrimaryAccountAvailable(identity_manager, "test@example.com",
                                      signin::ConsentLevel::kSignin);

  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);

  actions::ActionItem* your_chrome_section =
      root->GetChildren().children()[1]->GetActionItem();
  ASSERT_NE(your_chrome_section, nullptr);

  actions::BaseAction* profile_submenu = nullptr;
  for (const auto& child : your_chrome_section->GetChildren().children()) {
    if (child->GetActionItem()->GetActionId() == kActionProfileSubmenu) {
      profile_submenu = child.get();
      break;
    }
  }
  ASSERT_NE(profile_submenu, nullptr);
  EXPECT_EQ(profile_submenu->GetActionItem()->GetActionId(),
            kActionProfileSubmenu);
  EXPECT_EQ(profile_submenu->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kRow);

  // It should contain sync header, divider, primary actions, divider, header,
  // other profiles, divider, and footer actions.
  const auto& children = profile_submenu->GetChildren().children();
  ASSERT_EQ(children.size(), 12u);
  EXPECT_EQ(children[0]->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kHeader);
  EXPECT_EQ(
      children[0]->GetActionItem()->GetText(),
      l10n_util::GetStringFUTF16(IDS_PROFILE_ROW_SIGNED_IN_MESSAGE_WITH_EMAIL,
                                 {u"test@example.com"}));
  EXPECT_EQ(children[1]->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kDivider);
  EXPECT_EQ(children[2]->GetActionItem()->GetActionId(),
            kActionManageGoogleAccount);
  EXPECT_EQ(children[2]->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kRow);
  EXPECT_EQ(children[3]->GetActionItem()->GetActionId(),
            kActionCustomizeChrome);
  EXPECT_EQ(children[3]->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kRow);
  EXPECT_EQ(children[4]->GetActionItem()->GetActionId(), kActionCloseProfile);
  EXPECT_EQ(children[4]->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kRow);
  EXPECT_EQ(*children[4]->GetProperty(ActionAppMenuManager::kTextOverrideKey),
            l10n_util::GetPluralStringFUTF16(IDS_CLOSE_PROFILE,
                                             CountBrowsersFor(profile_.get())));
  EXPECT_EQ(children[5]->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kDivider);
  EXPECT_EQ(children[6]->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kHeader);
  EXPECT_EQ(children[6]->GetActionItem()->GetText(),
            l10n_util::GetStringUTF16(IDS_OTHER_CHROME_PROFILES_TITLE));
  EXPECT_EQ(children[7]->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kRow);
  EXPECT_EQ(children[7]->GetActionItem()->GetText(), u"Profile 2");
  EXPECT_EQ(children[8]->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kDivider);
  EXPECT_EQ(children[9]->GetActionItem()->GetActionId(), kActionAddNewProfile);
  EXPECT_EQ(children[9]->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kRow);
  EXPECT_EQ(children[10]->GetActionItem()->GetActionId(),
            kActionOpenGuestProfile);
  EXPECT_EQ(children[10]->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kRow);
  EXPECT_EQ(children[11]->GetActionItem()->GetActionId(),
            kActionManageChromeProfiles);
  EXPECT_EQ(children[11]->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kRow);
}

TEST_F(ActionAppMenuManagerTest, MAYBE_ProfileSubmenuSingleProfile) {
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  TestingProfile* profile1 = profile_manager.CreateTestingProfile("Profile 1");

  SyncServiceFactory::GetInstance()->SetTestingFactory(
      profile1,
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return std::make_unique<syncer::TestSyncService>();
          }));

  ON_CALL(mock_window_interface_, GetProfile())
      .WillByDefault(testing::Return(profile1));

  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);

  actions::ActionItem* your_chrome_section =
      root->GetChildren().children()[1]->GetActionItem();
  ASSERT_NE(your_chrome_section, nullptr);

  actions::BaseAction* profile_submenu = nullptr;
  for (const auto& child : your_chrome_section->GetChildren().children()) {
    if (child->GetActionItem()->GetActionId() == kActionProfileSubmenu) {
      profile_submenu = child.get();
      break;
    }
  }
  ASSERT_NE(profile_submenu, nullptr);

  // With a single profile and signed out, sync header and signin button are
  // present, followed by primary actions, divider, header, but no other
  // profiles and no trailing divider.
  const auto& children = profile_submenu->GetChildren().children();
  ASSERT_EQ(children.size(), 11u);
  EXPECT_EQ(children[0]->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kHeader);
  EXPECT_EQ(children[0]->GetActionItem()->GetText(),
            l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE));
  EXPECT_EQ(children[1]->GetActionItem()->GetActionId(), kActionShowSignin);
  EXPECT_EQ(children[2]->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kDivider);
  EXPECT_EQ(children[3]->GetActionItem()->GetActionId(),
            kActionManageGoogleAccount);
  EXPECT_EQ(children[4]->GetActionItem()->GetActionId(),
            kActionCustomizeChrome);
  EXPECT_EQ(children[5]->GetActionItem()->GetActionId(), kActionCloseProfile);
  EXPECT_EQ(children[6]->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kDivider);
  EXPECT_EQ(children[7]->GetActionItem()->GetProperty(
                ActionAppMenuManager::kDisplayTypeKey),
            ActionAppMenuManager::DisplayType::kHeader);
  EXPECT_EQ(children[7]->GetActionItem()->GetText(),
            l10n_util::GetStringUTF16(IDS_OTHER_CHROME_PROFILES_TITLE));
  EXPECT_EQ(children[8]->GetActionItem()->GetActionId(), kActionAddNewProfile);
  EXPECT_EQ(children[9]->GetActionItem()->GetActionId(),
            kActionOpenGuestProfile);
  EXPECT_EQ(children[10]->GetActionItem()->GetActionId(),
            kActionManageChromeProfiles);
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
