// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/standalone_browser_extension_app_context_menu.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/apps/app_service/app_service_proxy_chromeos.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/models/image_model.h"
#include "ui/views/vector_icons.h"

StandaloneBrowserExtensionAppContextMenu::
    StandaloneBrowserExtensionAppContextMenu(const std::string& app_id)
    : app_id_(app_id) {}
StandaloneBrowserExtensionAppContextMenu::
    ~StandaloneBrowserExtensionAppContextMenu() = default;

void StandaloneBrowserExtensionAppContextMenu::GetMenuModel(
    GetMenuModelCallback callback) {
  // TODO(https://crbug.com/1225848): Use Crosapi to help populate the shelf. In
  // the meanwhile, we do an asynchronous dispatch of callback.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneBrowserExtensionAppContextMenu::OnGetMenuModel,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void StandaloneBrowserExtensionAppContextMenu::ExecuteCommand(int command_id,
                                                              int event_flags) {
  if (command_id == ash::MENU_CLOSE) {
    // There can only be a single active ash profile when Lacros is running.
    apps::AppServiceProxyChromeOs* proxy =
        apps::AppServiceProxyFactory::GetForProfile(
            ProfileManager::GetPrimaryUserProfile());
    proxy->StopApp(app_id_);
    return;
  }
  // TODO(https://crbug.com/1225848): Implement all context menu items.
}

void StandaloneBrowserExtensionAppContextMenu::OnGetMenuModel(
    GetMenuModelCallback callback) {
  // TODO(https://crbug.com/1225848): Add the correct context menu items, which
  // are context dependent. These are:
  // Swap with next/previous (only when a11y enabled)
  // Pin/Unpin (depends on policy from lacros, and current pinned state)
  // Close (only if 1+ windows open)
  // Uninstall (only if allowed by policy)
  // App Info (only if allowed by app manifest)
  // Custom items
  auto model = std::make_unique<ui::SimpleMenuModel>(this);
  model->AddItemWithStringIdAndIcon(
      ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE,
      ui::ImageModel::FromVectorIcon(views::kCloseIcon,
                                     /*color_id=*/ui::kColorMenuIcon,
                                     ash::kAppContextMenuIconSize));
  std::move(callback).Run(std::move(model));
}
