// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_context_menu_controller.h"

#include "base/functional/callback_helpers.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/menu/menu_runner.h"

TabContextMenuController::TabContextMenuController(
    base::RepeatingCallback<bool(TabStripModel::ContextMenuCommand)>
        is_command_checked,
    base::RepeatingCallback<bool(TabStripModel::ContextMenuCommand)>
        is_command_enabled,
    base::RepeatingCallback<bool(TabStripModel::ContextMenuCommand)>
        is_command_alerted,
    base::RepeatingCallback<void(TabStripModel::ContextMenuCommand, int)>
        execute_command,
    base::RepeatingCallback<bool(int, ui::Accelerator*)> get_accelerator)
    : is_command_checked_(is_command_checked),
      is_command_enabled_(is_command_enabled),
      is_command_alerted_(is_command_alerted),
      execute_command_(execute_command),
      get_accelerator_(get_accelerator) {}

TabContextMenuController::~TabContextMenuController() = default;

void TabContextMenuController::LoadModel(
    std::unique_ptr<ui::SimpleMenuModel> model) {
  model_ = std::move(model);

  const int run_flags =
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU;

  menu_runner_ = std::make_unique<views::MenuRunner>(model_.get(), run_flags,
                                                     base::DoNothing());
}

void TabContextMenuController::RunMenuAt(const gfx::Point& point,
                                         ui::mojom::MenuSourceType source_type,
                                         views::Widget* widget) {
  menu_runner_->RunMenuAt(widget, nullptr, gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

void TabContextMenuController::CloseMenu() {
  if (menu_runner_) {
    menu_runner_->Cancel();
  }
}

bool TabContextMenuController::IsCommandIdChecked(int command_id) const {
  return is_command_checked_.Run(
      static_cast<TabStripModel::ContextMenuCommand>(command_id));
}

bool TabContextMenuController::IsCommandIdEnabled(int command_id) const {
  return is_command_enabled_.Run(
      static_cast<TabStripModel::ContextMenuCommand>(command_id));
}

bool TabContextMenuController::IsCommandIdAlerted(int command_id) const {
  return is_command_alerted_.Run(
      static_cast<TabStripModel::ContextMenuCommand>(command_id));
}

void TabContextMenuController::ExecuteCommand(int command_id, int event_flags) {
  execute_command_.Run(
      static_cast<TabStripModel::ContextMenuCommand>(command_id), event_flags);
}

bool TabContextMenuController::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return get_accelerator_.Run(command_id, accelerator);
}
