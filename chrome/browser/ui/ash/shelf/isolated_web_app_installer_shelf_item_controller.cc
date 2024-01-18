// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/isolated_web_app_installer_shelf_item_controller.h"

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/wm/window_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/ash/shelf/isolated_web_app_installer_context_menu.h"
#include "chrome/browser/ui/ash/shelf/lacros_shelf_item_controller.h"
#include "chrome/grit/generated_resources.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

// static
gfx::ImageSkia
IsolatedWebAppInstallerShelfItemController::GetDefaultInstallerShelfIcon() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  // TODO(crbug.com/1515460): Replace the placeholder default icon.
  return *rb.GetImageSkiaNamed(IDR_SETTINGS_LOGO_192);
}

IsolatedWebAppInstallerShelfItemController::
    IsolatedWebAppInstallerShelfItemController(const ash::ShelfID& shelf_id)
    : LacrosShelfItemController(shelf_id) {
  context_menu_ = std::make_unique<IsolatedWebAppInstallerContextMenu>(
      base::BindOnce(&IsolatedWebAppInstallerShelfItemController::Close,
                     weak_factory_.GetWeakPtr()));
}

IsolatedWebAppInstallerShelfItemController::
    ~IsolatedWebAppInstallerShelfItemController() {
  if (window_) {
    window_->RemoveObserver(this);
    window_ = nullptr;
  }
}

void IsolatedWebAppInstallerShelfItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback,
    const ItemFilterPredicate& filter_predicate) {
  if (!window_) {
    return;
  }
  // When the shelf item is clicked, bring it to the top.
  window_->Show();
  window_->Focus();
  std::move(callback).Run(ash::ShelfAction::SHELF_ACTION_WINDOW_ACTIVATED, {});
}

void IsolatedWebAppInstallerShelfItemController::GetContextMenu(
    int64_t display_id,
    GetContextMenuCallback callback) {
  context_menu_->GetMenuModel(std::move(callback));
}

void IsolatedWebAppInstallerShelfItemController::ExecuteCommand(
    bool from_context_menu,
    int64_t command_id,
    int32_t event_flags,
    int64_t display_id) {
  context_menu_->ExecuteCommand(command_id, event_flags);
}

void IsolatedWebAppInstallerShelfItemController::Close() {
  if (!window_) {
    return;
  }

  ash::window_util::CloseWidgetForWindow(window_);
}

void IsolatedWebAppInstallerShelfItemController::AddWindow(
    aura::Window* window) {
  // IsolatedWebAppInstallerShelfItemController supports only a single
  // window. However, multiple `AddWindow()` calls are allowed with the same
  // `window`.
  CHECK(!window_ || window_ == window);
  window_ = window;
  window_->AddObserver(this);
  UpdateShelfItem();
}

void IsolatedWebAppInstallerShelfItemController::OnWindowDestroying(
    aura::Window* window) {
  window_ = nullptr;
}

void IsolatedWebAppInstallerShelfItemController::UpdateShelfItem() {
  const ash::ShelfItem* current_item =
      ash::ShelfModel::Get()->ItemByID(shelf_id());
  CHECK(current_item);
  int index = ash::ShelfModel::Get()->ItemIndexByID(shelf_id());
  CHECK(index != -1);

  ash::ShelfItem updated_item = *current_item;
  updated_item.title =
      l10n_util::GetStringUTF16(IDS_IWA_INSTALLER_SHELF_ITEM_TITLE);
  updated_item.type = ash::TYPE_DIALOG;
  if (updated_item.image.isNull()) {
    updated_item.image = GetDefaultInstallerShelfIcon();
  }
  ash::ShelfModel::Get()->Set(index, updated_item);
}
