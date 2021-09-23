// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/standalone_browser_extension_app_context_menu.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
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
  // TODO(https://crbug.com/1225848): Implement all context menu items.
}

void StandaloneBrowserExtensionAppContextMenu::OnGetMenuModel(
    GetMenuModelCallback callback) {
  // TODO(https://crbug.com/1225848): Add the correct context menu items, which
  // are context dependent. For now MENU_CLOSE is just a placeholder, it has no
  // effect.
  auto model = std::make_unique<ui::SimpleMenuModel>(this);
  model->AddItemWithStringIdAndIcon(
      ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE,
      ui::ImageModel::FromVectorIcon(views::kCloseIcon, /*color_id=*/-1,
                                     ash::kAppContextMenuIconSize));
  std::move(callback).Run(std::move(model));
}
