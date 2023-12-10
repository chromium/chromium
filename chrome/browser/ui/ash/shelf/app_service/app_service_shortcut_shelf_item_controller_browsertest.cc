// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_service/app_service_shortcut_shelf_item_controller.h"

#include <memory>

#include "ash/public/cpp/shelf_model.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/apps/app_dialog/shortcut_removal_dialog_view.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "components/services/app_service/public/cpp/stub_icon_loader.h"
#include "components/vector_icons/vector_icons.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/vector_icons.h"
#include "url/gurl.h"

class AppServiceShortcutShelfItemControllerBrowserTest
    : public InProcessBrowserTest {
 protected:
  AppServiceShortcutShelfItemControllerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kCrosWebAppShortcutUiUpdate);
  }

  void SetUpOnMainThread() override { ASSERT_TRUE(controller()); }

  apps::ShortcutId CreateWebAppBasedShortcut(
      const GURL& shortcut_url,
      const std::u16string& shortcut_name,
      bool is_policy_install = false) {
    // Create web app based shortcut.
    auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
    web_app_info->start_url = shortcut_url;
    web_app_info->title = shortcut_name;
    auto local_shortcut_id = web_app::test::InstallWebApp(
        browser()->profile(), std::move(web_app_info),
        /*overwrite_existing_manifest_fields=*/true,
        is_policy_install ? webapps::WebappInstallSource::EXTERNAL_POLICY
                          : webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
    return apps::GenerateShortcutId(app_constants::kChromeAppId,
                                    local_shortcut_id);
  }

  ChromeShelfController* controller() {
    return ChromeShelfController::instance();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppServiceShortcutShelfItemControllerBrowserTest,
                       SelectItem) {
  GURL app_url = GURL("https://example.org/");
  std::u16string shortcut_name = u"Example";
  apps::ShortcutId shortcut_id =
      CreateWebAppBasedShortcut(app_url, shortcut_name);

  PinAppWithIDToShelf(shortcut_id.value());

  ash::ShelfItemDelegate* delegate =
      controller()->shelf_model()->GetShelfItemDelegate(
          ash::ShelfID(shortcut_id.value()));

  ASSERT_TRUE(delegate);

  ui_test_utils::UrlLoadObserver url_observer(
      app_url, content::NotificationService::AllSources());
  delegate->ItemSelected(/*event=*/nullptr, display::kInvalidDisplayId,
                         ash::LAUNCH_FROM_UNKNOWN,
                         /*callback=*/base::DoNothing(),
                         /*filter_predicate=*/base::NullCallback());
  url_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(AppServiceShortcutShelfItemControllerBrowserTest,
                       ContextMenuOpen) {
  GURL app_url = GURL("https://example.org/");
  std::u16string shortcut_name = u"Example";
  apps::ShortcutId shortcut_id =
      CreateWebAppBasedShortcut(app_url, shortcut_name);

  PinAppWithIDToShelf(shortcut_id.value());

  ash::ShelfItemDelegate* delegate =
      controller()->shelf_model()->GetShelfItemDelegate(
          ash::ShelfID(shortcut_id.value()));

  ASSERT_TRUE(delegate);

  base::test::TestFuture<std::unique_ptr<ui::SimpleMenuModel>> future;
  delegate->GetContextMenu(display::kDefaultDisplayId, future.GetCallback());

  std::unique_ptr<ui::SimpleMenuModel> menu_model = future.Take();

  auto launch_new_command_index =
      menu_model->GetIndexOfCommandId(ash::LAUNCH_NEW);
  ASSERT_TRUE(launch_new_command_index);
  EXPECT_EQ(launch_new_command_index.value(), 0u);

  std::u16string host_app_name;
  apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->AppRegistryCache()
      .ForOneApp(app_constants::kChromeAppId,
                 [&host_app_name](const apps::AppUpdate& update) {
                   host_app_name = base::UTF8ToUTF16(update.ShortName());
                 });

  EXPECT_EQ(u"Open " + shortcut_name + u" - " + host_app_name,
            menu_model->GetLabelAt(launch_new_command_index.value()));
  EXPECT_EQ(&vector_icons::kLaunchIcon,
            menu_model->GetIconAt(launch_new_command_index.value())
                .GetVectorIcon()
                .vector_icon());

  ui_test_utils::UrlLoadObserver url_observer(
      app_url, content::NotificationService::AllSources());
  menu_model->ActivatedAt(launch_new_command_index.value());
  url_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(AppServiceShortcutShelfItemControllerBrowserTest,
                       ContextMenuTooglePin) {
  GURL app_url = GURL("https://example.org/");
  std::u16string shortcut_name = u"Example";
  apps::ShortcutId shortcut_id =
      CreateWebAppBasedShortcut(app_url, shortcut_name);

  PinAppWithIDToShelf(shortcut_id.value());

  ash::ShelfItemDelegate* delegate =
      controller()->shelf_model()->GetShelfItemDelegate(
          ash::ShelfID(shortcut_id.value()));

  ASSERT_TRUE(delegate);

  base::test::TestFuture<std::unique_ptr<ui::SimpleMenuModel>> future;
  delegate->GetContextMenu(display::kDefaultDisplayId, future.GetCallback());

  std::unique_ptr<ui::SimpleMenuModel> menu_model = future.Take();

  auto tootle_pin_command_index =
      menu_model->GetIndexOfCommandId(ash::TOGGLE_PIN);
  ASSERT_TRUE(tootle_pin_command_index);
  EXPECT_EQ(tootle_pin_command_index.value(), 1u);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SHELF_CONTEXT_MENU_UNPIN),
            menu_model->GetLabelAt(tootle_pin_command_index.value()));
  EXPECT_EQ(&views::kUnpinIcon,
            menu_model->GetIconAt(tootle_pin_command_index.value())
                .GetVectorIcon()
                .vector_icon());

  menu_model->ActivatedAt(tootle_pin_command_index.value());

  EXPECT_FALSE(controller()->GetItem(ash::ShelfID(shortcut_id.value())));
}

// TODO(crbug.com/1485630):  Re-enable once segfault is resolved.
IN_PROC_BROWSER_TEST_F(AppServiceShortcutShelfItemControllerBrowserTest,
                       DISABLED_ContextMenuRemove) {
  GURL app_url = GURL("https://example.org/");
  std::u16string shortcut_name = u"Example";
  apps::ShortcutId shortcut_id =
      CreateWebAppBasedShortcut(app_url, shortcut_name);

  PinAppWithIDToShelf(shortcut_id.value());

  ash::ShelfItemDelegate* delegate =
      controller()->shelf_model()->GetShelfItemDelegate(
          ash::ShelfID(shortcut_id.value()));

  ASSERT_TRUE(delegate);

  base::test::TestFuture<std::unique_ptr<ui::SimpleMenuModel>> future;
  delegate->GetContextMenu(display::kDefaultDisplayId, future.GetCallback());

  std::unique_ptr<ui::SimpleMenuModel> menu_model = future.Take();

  auto uninstall_command_index =
      menu_model->GetIndexOfCommandId(ash::UNINSTALL);
  ASSERT_TRUE(uninstall_command_index);
  EXPECT_EQ(uninstall_command_index.value(), 2u);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_LIST_REMOVE_SHORTCUT),
            menu_model->GetLabelAt(uninstall_command_index.value()));
  EXPECT_EQ(&views::kUninstallIcon,
            menu_model->GetIconAt(uninstall_command_index.value())
                .GetVectorIcon()
                .vector_icon());

  apps::StubIconLoader shortcut_stub_icon_loader;
  apps::StubIconLoader app_stub_icon_loader;
  apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->OverrideShortcutInnerIconLoaderForTesting(&shortcut_stub_icon_loader);
  apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->OverrideInnerIconLoaderForTesting(&app_stub_icon_loader);
  shortcut_stub_icon_loader.update_version_by_app_id_[shortcut_id.value()] = 1;
  app_stub_icon_loader.update_version_by_app_id_[app_constants::kChromeAppId] =
      1;

  menu_model->ActivatedAt(uninstall_command_index.value());
  ShortcutRemovalDialogView* last_created_dialog =
      ShortcutRemovalDialogView::GetLastCreatedViewForTesting();
  last_created_dialog->AcceptDialog();

  base::RunLoop().RunUntilIdle();
  content::RunAllTasksUntilIdle();

  EXPECT_FALSE(apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
                   ->ShortcutRegistryCache()
                   ->HasShortcut(shortcut_id));
  EXPECT_FALSE(controller()->GetItem(ash::ShelfID(shortcut_id.value())));
}

IN_PROC_BROWSER_TEST_F(AppServiceShortcutShelfItemControllerBrowserTest,
                       PolicyNoContextMenuRemove) {
  GURL app_url = GURL("https://example.org/");
  std::u16string shortcut_name = u"Example";
  apps::ShortcutId shortcut_id = CreateWebAppBasedShortcut(
      app_url, shortcut_name, /*is_policy_install = */ true);

  PinAppWithIDToShelf(shortcut_id.value());

  ash::ShelfItemDelegate* delegate =
      controller()->shelf_model()->GetShelfItemDelegate(
          ash::ShelfID(shortcut_id.value()));

  ASSERT_TRUE(delegate);

  base::test::TestFuture<std::unique_ptr<ui::SimpleMenuModel>> future;
  delegate->GetContextMenu(display::kDefaultDisplayId, future.GetCallback());

  std::unique_ptr<ui::SimpleMenuModel> menu_model = future.Take();

  auto uninstall_command_index =
      menu_model->GetIndexOfCommandId(ash::UNINSTALL);
  EXPECT_FALSE(uninstall_command_index);
}
