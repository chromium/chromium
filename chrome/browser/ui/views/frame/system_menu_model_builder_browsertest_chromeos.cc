// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/settings_window_manager_observer_chromeos.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/models/menu_model.h"

using chrome::SettingsWindowManager;
using chrome::SettingsWindowManagerObserver;
using chromeos::ProfileHelper;
using user_manager::UserManager;

namespace {

class SystemMenuModelBuilderMultiUserTest
    : public chromeos::LoginManagerTest,
      public SettingsWindowManagerObserver {
 public:
  SystemMenuModelBuilderMultiUserTest()
      : LoginManagerTest(/*should_launch_browser=*/false,
                         /*should_initialize_webui=*/false),
        account_id1_(
            AccountId::FromUserEmailGaiaId("user1@gmail.com", "1111111111")),
        account_id2_(
            AccountId::FromUserEmailGaiaId("user2@gmail.com", "2222222222")) {}
  ~SystemMenuModelBuilderMultiUserTest() override = default;

  // SettingsWindowManagerObserver:
  void OnNewSettingsWindow(Browser* settings_browser) override {
    settings_browser_ = settings_browser;
  }

 protected:
  const AccountId account_id1_;
  const AccountId account_id2_;
  Browser* settings_browser_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(SystemMenuModelBuilderMultiUserTest,
                       PRE_MultiUserSettingsWindowFrameMenu) {
  RegisterUser(account_id1_);
  RegisterUser(account_id2_);
  chromeos::StartupUtils::MarkOobeCompleted();
}

// Regression test for https://crbug.com/1023043
IN_PROC_BROWSER_TEST_F(SystemMenuModelBuilderMultiUserTest,
                       MultiUserSettingsWindowFrameMenu) {
  // Log in 2 users.
  LoginUser(account_id1_);
  base::RunLoop().RunUntilIdle();
  chromeos::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  base::RunLoop().RunUntilIdle();

  // Install the Settings App.
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(
      UserManager::Get()->FindUser(account_id1_));
  web_app::WebAppProvider::Get(profile)
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  // Open the settings window and record the |settings_browser_|.
  auto* manager = SettingsWindowManager::GetInstance();
  manager->AddObserver(this);
  manager->ShowOSSettings(profile);
  manager->RemoveObserver(this);
  ASSERT_TRUE(settings_browser_);

  // Copy the command ids from the system menu.
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(settings_browser_);
  ui::MenuModel* menu = browser_view->frame()->GetSystemMenuModel();
  std::set<int> commands;
  for (int i = 0; i < menu->GetItemCount(); i++)
    commands.insert(menu->GetCommandIdAt(i));

  // Standard WebUI commands are available.
  EXPECT_TRUE(base::Contains(commands, IDC_BACK));
  EXPECT_TRUE(base::Contains(commands, IDC_FORWARD));
  EXPECT_TRUE(base::Contains(commands, IDC_RELOAD));

  // Settings window cannot be teleported.
  EXPECT_FALSE(base::Contains(commands, IDC_VISIT_DESKTOP_OF_LRU_USER_2));
  EXPECT_FALSE(base::Contains(commands, IDC_VISIT_DESKTOP_OF_LRU_USER_3));
}

}  // namespace
