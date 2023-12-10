// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_service/app_service_shortcut_shelf_context_menu.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/shelf_item.h"
#include "base/functional/callback.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/vector_icons.h"

AppServiceShortcutShelfContextMenu::AppServiceShortcutShelfContextMenu(
    ChromeShelfController* controller,
    const ash::ShelfItem* item,
    int64_t display_id)
    : ShelfContextMenu(controller, item, display_id) {
  shortcut_id_ = apps::ShortcutId(item->id.app_id);
}

AppServiceShortcutShelfContextMenu::~AppServiceShortcutShelfContextMenu() =
    default;

void AppServiceShortcutShelfContextMenu::GetMenuModel(
    GetMenuModelCallback callback) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(controller()->profile());
  if (!proxy->ShortcutRegistryCache()->HasShortcut(shortcut_id_)) {
    std::move(callback).Run(nullptr);
    return;
  }
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);

  // Open shortcut in its host app.
  apps::ShortcutView shortcut =
      proxy->ShortcutRegistryCache()->GetShortcut(shortcut_id_);
  std::u16string shortcut_name = base::UTF8ToUTF16(shortcut->name.value());
  std::u16string host_app_name;
  proxy->AppRegistryCache().ForOneApp(
      shortcut->host_app_id, [&host_app_name](const apps::AppUpdate& update) {
        host_app_name = base::UTF8ToUTF16(update.ShortName());
      });

  std::u16string open_label = l10n_util::GetStringFUTF16(
      IDS_APP_CONTEXT_MENU_LAUNCH_APP_SHORTCUT, shortcut_name, host_app_name);
  menu_model->AddItemWithIcon(
      ash::LAUNCH_NEW, open_label,
      ui::ImageModel::FromVectorIcon(vector_icons::kLaunchIcon,
                                     ui::kColorAshSystemUIMenuIcon,
                                     ash::kAppContextMenuIconSize));

  // Pin/unpin shortcut from shelf.
  AddPinMenu(menu_model.get());

  // Remove shortcut.
  if (shortcut->allow_removal.value_or(true)) {
    AddContextMenuOption(menu_model.get(),
                         static_cast<ash::CommandId>(ash::UNINSTALL),
                         IDS_APP_LIST_REMOVE_SHORTCUT);
  }

  std::move(callback).Run(std::move(menu_model));
}

void AppServiceShortcutShelfContextMenu::ExecuteCommand(int command_id,
                                                        int event_flags) {
  if (command_id == ash::UNINSTALL) {
    apps::AppServiceProxy* proxy =
        apps::AppServiceProxyFactory::GetForProfile(controller()->profile());
    if (!proxy->ShortcutRegistryCache()->HasShortcut(shortcut_id_)) {
      return;
    }
    proxy->RemoveShortcut(shortcut_id_, apps::UninstallSource::kShelf,
                          nullptr /* parent_window */);
    return;
  }
  ExecuteCommonCommand(command_id, event_flags);
}
