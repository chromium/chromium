// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/display/display.h"
#include "ui/views/vector_icons.h"

class AppServiceShelfContextMenuBrowserTest : public InProcessBrowserTest {
 public:
  AppServiceShelfContextMenuBrowserTest() = default;
  ~AppServiceShelfContextMenuBrowserTest() override = default;

  struct MenuSection {
    std::unique_ptr<ui::SimpleMenuModel> menu_model;
    ui::MenuModel* sub_model = nullptr;
    size_t command_index = 0;
  };

  absl::optional<MenuSection> GetContextMenuSectionForAppCommand(
      const web_app::AppId& app_id,
      int command_id) {
    MenuSection result;
    ash::ShelfModel* shelf_model = ash::ShelfModel::Get();
    PinAppWithIDToShelf(app_id);
    ash::ShelfItemDelegate* delegate =
        shelf_model->GetShelfItemDelegate(ash::ShelfID(app_id));
    base::RunLoop run_loop;
    delegate->GetContextMenu(
        display::Display::GetDefaultDisplay().id(),
        base::BindLambdaForTesting(
            [&run_loop, &result](std::unique_ptr<ui::SimpleMenuModel> model) {
              result.menu_model = std::move(model);
              run_loop.Quit();
            }));
    run_loop.Run();

    result.sub_model = result.menu_model.get();
    result.command_index = 0;
    if (!ui::MenuModel::GetModelAndIndexForCommandId(
            command_id, &result.sub_model, &result.command_index)) {
      return absl::nullopt;
    }

    return result;
  }
};

class AppServiceShelfContextMenuWebAppBrowserTest
    : public AppServiceShelfContextMenuBrowserTest {
 public:
  AppServiceShelfContextMenuWebAppBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesktopPWAsTabStrip,
         features::kDesktopPWAsTabStripSettings},
        {});
  }
  ~AppServiceShelfContextMenuWebAppBrowserTest() override = default;

  const gfx::VectorIcon& GetExpectedLaunchNewIcon(int command_id) {
    if (command_id == ash::USE_LAUNCH_TYPE_REGULAR)
      return views::kNewTabIcon;
    else if (command_id == ash::USE_LAUNCH_TYPE_WINDOW)
      return views::kNewWindowIcon;
    else
      return views::kOpenIcon;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppServiceShelfContextMenuWebAppBrowserTest,
                       WindowCommandCheckedForMinimalUi) {
  Profile* profile = browser()->profile();
  base::UserActionTester user_action_tester;

  auto web_app_install_info = std::make_unique<WebAppInstallInfo>();
  web_app_install_info->start_url = GURL("https://example.org");
  web_app_install_info->display_mode = blink::mojom::DisplayMode::kMinimalUi;
  web_app::AppId app_id =
      web_app::test::InstallWebApp(profile, std::move(web_app_install_info));

  // Activate open in window menu item.
  absl::optional<MenuSection> menu_section =
      GetContextMenuSectionForAppCommand(app_id, ash::USE_LAUNCH_TYPE_WINDOW);
  ASSERT_TRUE(menu_section);
  menu_section->sub_model->ActivatedAt(menu_section->command_index);
  web_app::WebAppProvider::GetForTest(profile)
      ->command_manager()
      .AwaitAllCommandsCompleteForTesting();

  EXPECT_EQ(user_action_tester.GetActionCount("WebApp.SetWindowMode.Window"),
            1);

  // Open in window should be checked after activating it.
  EXPECT_TRUE(
      menu_section->sub_model->IsItemCheckedAt(menu_section->command_index));
}

IN_PROC_BROWSER_TEST_F(AppServiceShelfContextMenuWebAppBrowserTest,
                       SetOpenInTabbedWindow) {
  Profile* profile = browser()->profile();
  base::UserActionTester user_action_tester;

  auto web_app_install_info = std::make_unique<WebAppInstallInfo>();
  web_app_install_info->start_url = GURL("https://example.org");
  web_app_install_info->display_mode = blink::mojom::DisplayMode::kMinimalUi;
  web_app::AppId app_id =
      web_app::test::InstallWebApp(profile, std::move(web_app_install_info));

  // Set app to open in tabbed window.
  absl::optional<MenuSection> menu_section = GetContextMenuSectionForAppCommand(
      app_id, ash::USE_LAUNCH_TYPE_TABBED_WINDOW);
  ASSERT_TRUE(menu_section);
  menu_section->sub_model->ActivatedAt(menu_section->command_index);
  web_app::WebAppProvider::GetForTest(profile)
      ->command_manager()
      .AwaitAllCommandsCompleteForTesting();

  EXPECT_EQ(user_action_tester.GetActionCount("WebApp.SetWindowMode.Tabbed"),
            1);

  // App window should have tab strip.
  Browser* app_browser = web_app::LaunchWebAppBrowser(profile, app_id);
  EXPECT_TRUE(app_browser->app_controller()->has_tab_strip());
}

IN_PROC_BROWSER_TEST_F(AppServiceShelfContextMenuWebAppBrowserTest,
                       SetOpenInBrowserTab) {
  Profile* profile = browser()->profile();
  base::UserActionTester user_action_tester;

  auto web_app_install_info = std::make_unique<WebAppInstallInfo>();
  web_app_install_info->start_url = GURL("https://example.org");
  web_app::AppId app_id =
      web_app::test::InstallWebApp(profile, std::move(web_app_install_info));

  // Set app to open in browser tab.
  absl::optional<MenuSection> menu_section =
      GetContextMenuSectionForAppCommand(app_id, ash::USE_LAUNCH_TYPE_REGULAR);
  ASSERT_TRUE(menu_section);
  menu_section->sub_model->ActivatedAt(menu_section->command_index);
  web_app::WebAppProvider::GetForTest(profile)
      ->command_manager()
      .AwaitAllCommandsCompleteForTesting();

  EXPECT_EQ(user_action_tester.GetActionCount("WebApp.SetWindowMode.Tab"), 1);
}

IN_PROC_BROWSER_TEST_F(AppServiceShelfContextMenuWebAppBrowserTest,
                       LaunchNewMenuItemDynamicallyChanges) {
  Profile* profile = browser()->profile();
  auto web_app_install_info = std::make_unique<WebAppInstallInfo>();
  web_app_install_info->start_url = GURL("https://example.org");
  web_app::AppId app_id =
      web_app::test::InstallWebApp(profile, std::move(web_app_install_info));

  absl::optional<MenuSection> menu_section =
      GetContextMenuSectionForAppCommand(app_id, ash::LAUNCH_NEW);
  ASSERT_TRUE(menu_section);

  auto* launch_new_submodel =
      menu_section->menu_model->GetSubmenuModelAt(menu_section->command_index);

  EXPECT_GT(launch_new_submodel->GetItemCount(), 0u);
  for (size_t launch_new_item_index = 0;
       launch_new_item_index < launch_new_submodel->GetItemCount();
       ++launch_new_item_index) {
    const auto label_from_submenu =
        launch_new_submodel->GetLabelAt(launch_new_item_index);
    launch_new_submodel->ActivatedAt(launch_new_item_index);
    web_app::WebAppProvider::GetForTest(profile)
        ->command_manager()
        .AwaitAllCommandsCompleteForTesting();
    EXPECT_TRUE(launch_new_submodel->IsItemCheckedAt(launch_new_item_index));

    // Parent `LAUNCH_NEW` item label and icon change dynamically after
    // selection.
    EXPECT_EQ(menu_section->menu_model->GetLabelAt(menu_section->command_index),
              label_from_submenu);
    EXPECT_EQ(menu_section->menu_model->GetIconAt(menu_section->command_index)
                  .GetVectorIcon()
                  .vector_icon(),
              &GetExpectedLaunchNewIcon(
                  launch_new_submodel->GetCommandIdAt(launch_new_item_index)));
  }
}

class AppServiceShelfContextMenuCrostiniAppBrowserTest
    : public AppServiceShelfContextMenuBrowserTest {
 public:
  AppServiceShelfContextMenuCrostiniAppBrowserTest() = default;
  ~AppServiceShelfContextMenuCrostiniAppBrowserTest() override = default;

  std::string InstallCrostiniApp() {
    vm_tools::apps::ApplicationList crostini_list;
    crostini_list.set_vm_name(crostini::kCrostiniDefaultVmName);
    crostini_list.set_container_name(crostini::kCrostiniDefaultContainerName);
    *crostini_list.add_apps() = crostini::CrostiniTestHelper::BasicApp(
        "app-service-context-menu-test-app");

    guest_os::GuestOsRegistryServiceFactory::GetForProfile(browser()->profile())
        ->UpdateApplicationList(crostini_list);

    return crostini::CrostiniTestHelper::GenerateAppId(
        "app-service-context-menu-test-app", crostini::kCrostiniDefaultVmName,
        crostini::kCrostiniDefaultContainerName);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kCrostini};
};

// Crostini apps have `LAUNCH_NEW` menu item at non-0 position without submenu.
// Make sure there is no crash.
IN_PROC_BROWSER_TEST_F(AppServiceShelfContextMenuCrostiniAppBrowserTest,
                       LaunchNewForCrostiniApps) {
  auto app_id = InstallCrostiniApp();
  auto menu_section =
      GetContextMenuSectionForAppCommand(app_id, ash::LAUNCH_NEW);
  ASSERT_TRUE(menu_section);

  EXPECT_GT(menu_section->command_index, 0u);
  EXPECT_FALSE(
      menu_section->menu_model->GetSubmenuModelAt(menu_section->command_index));
  EXPECT_EQ(menu_section->menu_model->GetLabelAt(menu_section->command_index),
            u"Open");
  EXPECT_EQ(menu_section->menu_model->GetIconAt(menu_section->command_index)
                .GetVectorIcon()
                .vector_icon(),
            &views::kOpenIcon);
}

IN_PROC_BROWSER_TEST_F(AppServiceShelfContextMenuCrostiniAppBrowserTest,
                       ShutDownGuestOs) {
  ash::SystemWebAppManager::Get(browser()->profile())
      ->InstallSystemAppsForTesting();
  auto menu_section = GetContextMenuSectionForAppCommand(
      guest_os::kTerminalSystemAppId, ash::SHUTDOWN_GUEST_OS);
  ASSERT_FALSE(menu_section);

  auto* crostini_manager =
      crostini::CrostiniManager::GetForProfile(browser()->profile());
  crostini_manager->AddRunningVmForTesting(crostini::kCrostiniDefaultVmName);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  menu_section = GetContextMenuSectionForAppCommand(
      guest_os::kTerminalSystemAppId, ash::SHUTDOWN_GUEST_OS);
  ASSERT_TRUE(menu_section);

  EXPECT_GT(menu_section->command_index, 0u);
  EXPECT_FALSE(
      menu_section->menu_model->GetSubmenuModelAt(menu_section->command_index));
  EXPECT_EQ(menu_section->menu_model->GetLabelAt(menu_section->command_index),
            u"Shut down Linux");
  EXPECT_EQ(menu_section->menu_model->GetIconAt(menu_section->command_index)
                .GetVectorIcon()
                .vector_icon(),
            &kShutdownGuestOsIcon);
}
