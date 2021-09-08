// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_apps.h"

#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/menu_item_constants.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/webapps/browser/installable/installable_metrics.h"

using apps::IconEffects;

namespace web_app {

void WebApps::Uninstall(const std::string& app_id,
                        apps::mojom::UninstallSource uninstall_source,
                        bool clear_site_data,
                        bool report_abuse) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  publisher_helper().UninstallWebApp(web_app, uninstall_source, clear_site_data,
                                     report_abuse);
}

void WebApps::PauseApp(const std::string& app_id) {
  publisher_helper().PauseApp(app_id);
}

void WebApps::UnpauseApp(const std::string& app_id) {
  publisher_helper().UnpauseApp(app_id);
}

void WebApps::GetMenuModel(const std::string& app_id,
                           apps::mojom::MenuType menu_type,
                           int64_t display_id,
                           GetMenuModelCallback callback) {
  bool is_system_web_app = false;
  bool can_use_uninstall = true;
  apps::mojom::WindowMode display_mode;
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->AppRegistryCache()
      .ForOneApp(app_id, [&is_system_web_app, &can_use_uninstall,
                          &display_mode](const apps::AppUpdate& update) {
        if (update.InstallSource() == apps::mojom::InstallSource::kSystem) {
          is_system_web_app = true;
        }
        if (update.InstallSource() == apps::mojom::InstallSource::kSystem ||
            update.InstallSource() == apps::mojom::InstallSource::kPolicy) {
          can_use_uninstall = false;
        }
        display_mode = update.WindowMode();
      });

  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();

  if (is_system_web_app) {
    const auto* web_app = GetWebApp(app_id);
    DCHECK(web_app);
    DCHECK(web_app->client_data().system_web_app_data.has_value());

    SystemAppType swa_type =
        web_app->client_data().system_web_app_data->system_app_type;

    auto* system_app = WebAppProvider::GetForSystemWebApps(profile())
                           ->system_web_app_manager()
                           .GetSystemApp(swa_type);
    if (system_app && system_app->ShouldShowNewWindowMenuOption()) {
      apps::AddCommandItem(ash::MENU_OPEN_NEW,
                           IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW, &menu_items);
    }
  } else {
    apps::CreateOpenNewSubmenu(menu_type,
                               display_mode == apps::mojom::WindowMode::kBrowser
                                   ? IDS_APP_LIST_CONTEXT_MENU_NEW_TAB
                                   : IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW,
                               &menu_items);
  }

  if (menu_type == apps::mojom::MenuType::kShelf &&
      instance_registry_->ContainsAppId(app_id)) {
    apps::AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE,
                         &menu_items);
  }

  if (can_use_uninstall) {
    apps::AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM,
                         &menu_items);
  }

  if (!is_system_web_app) {
    apps::AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                         &menu_items);
  }

  GetMenuModelFromWebAppProvider(app_id, std::move(menu_items),
                                 std::move(callback));
}

void WebApps::GetMenuModelFromWebAppProvider(
    const std::string& app_id,
    apps::mojom::MenuItemsPtr menu_items,
    GetMenuModelCallback callback) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(callback).Run(apps::mojom::MenuItems::New());
    return;
  }

  // Read shortcuts menu item icons from disk, if any.
  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenuUI) &&
      !web_app->shortcuts_menu_item_infos().empty()) {
    provider()->icon_manager().ReadAllShortcutsMenuIcons(
        app_id, base::BindOnce(&WebApps::OnShortcutsMenuIconsRead,
                               base::AsWeakPtr<WebApps>(this), app_id,
                               std::move(menu_items), std::move(callback)));
  } else {
    std::move(callback).Run(std::move(menu_items));
  }
}

void WebApps::OnShortcutsMenuIconsRead(
    const std::string& app_id,
    apps::mojom::MenuItemsPtr menu_items,
    GetMenuModelCallback callback,
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(callback).Run(apps::mojom::MenuItems::New());
    return;
  }

  apps::AddSeparator(ui::DOUBLE_SEPARATOR, &menu_items);

  size_t menu_item_index = 0;

  for (const WebApplicationShortcutsMenuItemInfo& menu_item_info :
       web_app->shortcuts_menu_item_infos()) {
    const std::map<SquareSizePx, SkBitmap>* menu_item_icon_bitmaps = nullptr;
    if (menu_item_index < shortcuts_menu_icon_bitmaps.size()) {
      // We prefer |MASKABLE| icons, but fall back to icons with purpose |ANY|.
      menu_item_icon_bitmaps =
          &shortcuts_menu_icon_bitmaps[menu_item_index].maskable;
      if (menu_item_icon_bitmaps->empty()) {
        menu_item_icon_bitmaps =
            &shortcuts_menu_icon_bitmaps[menu_item_index].any;
      }
    }

    if (menu_item_index != 0) {
      apps::AddSeparator(ui::PADDED_SEPARATOR, &menu_items);
    }

    gfx::ImageSkia icon;
    if (menu_item_icon_bitmaps) {
      IconEffects icon_effects = IconEffects::kNone;

      // We apply masking to each shortcut icon, regardless if the purpose is
      // |MASKABLE| or |ANY|.
      icon_effects = apps::kCrOsStandardBackground | apps::kCrOsStandardMask;

      icon = ConvertSquareBitmapsToImageSkia(
          *menu_item_icon_bitmaps, icon_effects,
          /*size_hint_in_dip=*/apps::kAppShortcutIconSizeDip);
    }

    // Uses integer |command_id| to store menu item index.
    const int command_id = ash::LAUNCH_APP_SHORTCUT_FIRST + menu_item_index;

    const std::string label = base::UTF16ToUTF8(menu_item_info.name);
    std::string shortcut_id = publisher_helper().GenerateShortcutId();
    publisher_helper().StoreShortcutId(shortcut_id, menu_item_info);

    apps::AddShortcutCommandItem(command_id, shortcut_id, label, icon,
                                 &menu_items);

    ++menu_item_index;
  }

  std::move(callback).Run(std::move(menu_items));
}

void WebApps::ExecuteContextMenuCommand(const std::string& app_id,
                                        int command_id,
                                        const std::string& shortcut_id,
                                        int64_t display_id) {
  publisher_helper().ExecuteContextMenuCommand(app_id, shortcut_id, display_id);
}

void WebApps::SetWindowMode(const std::string& app_id,
                            apps::mojom::WindowMode window_mode) {
  publisher_helper().SetWindowMode(app_id, window_mode);
}

}  // namespace web_app
