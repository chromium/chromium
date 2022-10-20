// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/ui/user_adding_screen.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/menu_model.h"

using ::ash::ProfileHelper;
using chrome::SettingsWindowManager;
using user_manager::UserManager;

namespace {

class SystemMenuModelBuilderMultiUserTest : public ash::LoginManagerTest {
 public:
  SystemMenuModelBuilderMultiUserTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(2);
    account_id1_ = login_mixin_.users()[0].account_id;
    account_id2_ = login_mixin_.users()[1].account_id;
  }
  ~SystemMenuModelBuilderMultiUserTest() override = default;

 protected:
  AccountId account_id1_;
  AccountId account_id2_;
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
};

// Regression test for https://crbug.com/1023043
IN_PROC_BROWSER_TEST_F(SystemMenuModelBuilderMultiUserTest,
                       MultiUserSettingsWindowFrameMenu) {
  // Log in 2 users.
  LoginUser(account_id1_);
  base::RunLoop().RunUntilIdle();
  ash::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  base::RunLoop().RunUntilIdle();

  // Install the Settings App.
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(
      UserManager::Get()->FindUser(account_id1_));
  ash::SystemWebAppManager::GetForTest(profile)->InstallSystemAppsForTesting();

  // Open the settings window and record the |settings_browser|.
  auto* manager = SettingsWindowManager::GetInstance();
  ui_test_utils::BrowserChangeObserver browser_opened(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  manager->ShowOSSettings(profile);
  browser_opened.Wait();

  auto* settings_browser = manager->FindBrowserForProfile(profile);
  ASSERT_TRUE(settings_browser);

  // Copy the command ids from the system menu.
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(settings_browser);
  ui::MenuModel* menu = browser_view->frame()->GetSystemMenuModel();
  std::set<int> commands;
  for (size_t i = 0; i < menu->GetItemCount(); ++i)
    commands.insert(menu->GetCommandIdAt(i));

  // Standard WebUI commands are available.
  EXPECT_TRUE(base::Contains(commands, IDC_BACK));
  EXPECT_TRUE(base::Contains(commands, IDC_FORWARD));
  EXPECT_TRUE(base::Contains(commands, IDC_RELOAD));

  // Settings window cannot be teleported.
  EXPECT_FALSE(base::Contains(commands, IDC_VISIT_DESKTOP_OF_LRU_USER_2));
  EXPECT_FALSE(base::Contains(commands, IDC_VISIT_DESKTOP_OF_LRU_USER_3));
  EXPECT_FALSE(base::Contains(commands, IDC_VISIT_DESKTOP_OF_LRU_USER_4));
  EXPECT_FALSE(base::Contains(commands, IDC_VISIT_DESKTOP_OF_LRU_USER_5));
}

}  // namespace
