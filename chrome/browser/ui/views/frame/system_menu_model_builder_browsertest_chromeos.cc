// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/menu_model.h"
#include "url/gurl.h"

using ::ash::ProfileHelper;
using chrome::SettingsWindowManager;
using user_manager::UserManager;

namespace {

// Returns true if there exists a command with specified id in the given menu.
// False otherwise.
bool ContainsCommandIdInMenu(int command_id, const ui::MenuModel* menu) {
  CHECK(menu);
  for (size_t index = 0; index < menu->GetItemCount(); index++) {
    if (menu->GetCommandIdAt(index) == command_id) {
      return true;
    }
  }
  return false;
}

// Browser tests for verifying `SystemMenuModelBuilder` behavior for apps when
// locked (and not locked) for OnTask. Only relevant for non-web browser
// scenarios.
class SystemMenuModelBuilderWithOnTaskTest : public InProcessBrowserTest {
 protected:
  webapps::AppId InstallMockApp() {
    return web_app::test::InstallDummyWebApp(
        browser()->profile(), /*app_name=*/"Mock app",
        /*app_url=*/GURL("https://www.example.com/"));
  }
};

IN_PROC_BROWSER_TEST_F(SystemMenuModelBuilderWithOnTaskTest,
                       SystemMenuWhenNotLockedForOnTask) {
  // Install and launch app.
  webapps::AppId app_id = InstallMockApp();
  Browser* const app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  app_browser->SetLockedForOnTask(false);

  // Retrieve system menu.
  const BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(app_browser);
  const ui::MenuModel* const menu = browser_view->frame()->GetSystemMenuModel();

  // Verify system menu command availability.
  EXPECT_TRUE(ContainsCommandIdInMenu(IDC_BACK, menu));
  EXPECT_TRUE(ContainsCommandIdInMenu(IDC_FORWARD, menu));
  EXPECT_TRUE(ContainsCommandIdInMenu(IDC_RELOAD, menu));
  EXPECT_TRUE(ContainsCommandIdInMenu(IDC_TASK_MANAGER, menu));
}

IN_PROC_BROWSER_TEST_F(SystemMenuModelBuilderWithOnTaskTest,
                       SystemMenuWhenLockedForOnTask) {
  // Install and launch app.
  webapps::AppId app_id = InstallMockApp();
  Browser* const app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  app_browser->SetLockedForOnTask(true);

  // Retrieve system menu.
  const BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(app_browser);
  const ui::MenuModel* const menu = browser_view->frame()->GetSystemMenuModel();

  // Verify system menu command availability.
  EXPECT_TRUE(ContainsCommandIdInMenu(IDC_BACK, menu));
  EXPECT_TRUE(ContainsCommandIdInMenu(IDC_FORWARD, menu));
  EXPECT_TRUE(ContainsCommandIdInMenu(IDC_RELOAD, menu));
  EXPECT_FALSE(ContainsCommandIdInMenu(IDC_TASK_MANAGER, menu));
}

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

  // Retrieve the system menu so we can verify command availability.
  const BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(settings_browser);
  const ui::MenuModel* const menu = browser_view->frame()->GetSystemMenuModel();

  // Standard WebUI commands are available.
  EXPECT_TRUE(ContainsCommandIdInMenu(IDC_BACK, menu));
  EXPECT_TRUE(ContainsCommandIdInMenu(IDC_FORWARD, menu));
  EXPECT_TRUE(ContainsCommandIdInMenu(IDC_RELOAD, menu));

  // Task manager should also be available.
  EXPECT_TRUE(ContainsCommandIdInMenu(IDC_TASK_MANAGER, menu));

  // Settings window cannot be teleported.
  EXPECT_FALSE(ContainsCommandIdInMenu(IDC_VISIT_DESKTOP_OF_LRU_USER_2, menu));
  EXPECT_FALSE(ContainsCommandIdInMenu(IDC_VISIT_DESKTOP_OF_LRU_USER_3, menu));
  EXPECT_FALSE(ContainsCommandIdInMenu(IDC_VISIT_DESKTOP_OF_LRU_USER_4, menu));
  EXPECT_FALSE(ContainsCommandIdInMenu(IDC_VISIT_DESKTOP_OF_LRU_USER_5, menu));
}

}  // namespace
