// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab/tab_context_menu_controller.h"

#include "base/functional/callback_helpers.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/menu/menu_runner.h"

TabContextMenuController::TabContextMenuController(int index,
                                                   Delegate* delegate)
    : tab_index_(index), delegate_(delegate) {}

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
  return delegate_->IsContextMenuCommandChecked(
      static_cast<TabStripModel::ContextMenuCommand>(command_id));
}

bool TabContextMenuController::IsCommandIdEnabled(int command_id) const {
  return delegate_->IsContextMenuCommandEnabled(
      tab_index_, static_cast<TabStripModel::ContextMenuCommand>(command_id));
}

bool TabContextMenuController::IsCommandIdAlerted(int command_id) const {
  return delegate_->IsContextMenuCommandAlerted(
      static_cast<TabStripModel::ContextMenuCommand>(command_id));
}

void TabContextMenuController::ExecuteCommand(int command_id, int event_flags) {
  delegate_->ExecuteContextMenuCommand(
      tab_index_, static_cast<TabStripModel::ContextMenuCommand>(command_id),
      event_flags);
}

bool TabContextMenuController::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return delegate_->GetContextMenuAccelerator(command_id, accelerator);
}
