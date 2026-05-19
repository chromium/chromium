// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab/tab_context_menu_controller.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/menu/menu_runner.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "content/public/browser/context_menu_params.h"
#endif

TabContextMenuController::TabContextMenuController(tabs::TabHandle tab_handle,
                                                   Delegate* delegate)
    : tab_handle_(tab_handle), delegate_(delegate) {}

TabContextMenuController::~TabContextMenuController() = default;

void TabContextMenuController::LoadModel(
    std::unique_ptr<TabMenuModel> model,
    base::RepeatingClosure on_menu_closed) {
  tab_menu_model_ = model.get();
  LoadModel(std::unique_ptr<ui::SimpleMenuModel>(std::move(model)),
            std::move(on_menu_closed));
}

void TabContextMenuController::LoadModel(
    std::unique_ptr<ui::SimpleMenuModel> model,
    base::RepeatingClosure on_menu_closed) {
  model_ = std::move(model);

  const int run_flags =
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU;

  menu_runner_ = std::make_unique<views::MenuRunner>(model_.get(), run_flags,
                                                     std::move(on_menu_closed));
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
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(command_id)) {
    if (tab_menu_model_ && tab_menu_model_->extension_items()) {
      return tab_menu_model_->extension_items()->IsCommandIdChecked(command_id);
    }
    return false;
  }
#endif
  return delegate_->IsContextMenuCommandChecked(
      static_cast<TabStripModel::ContextMenuCommand>(command_id));
}

bool TabContextMenuController::IsCommandIdEnabled(int command_id) const {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(command_id)) {
    if (tab_menu_model_ && tab_menu_model_->extension_items()) {
      return tab_menu_model_->extension_items()->IsCommandIdEnabled(command_id);
    }
    return false;
  }
#endif
  if (auto* tab = tab_handle_.Get()) {
    return delegate_->IsContextMenuCommandEnabled(
        tab, static_cast<TabStripModel::ContextMenuCommand>(command_id));
  }
  return false;
}

bool TabContextMenuController::IsCommandIdVisible(int command_id) const {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(command_id)) {
    if (tab_menu_model_ && tab_menu_model_->extension_items()) {
      return tab_menu_model_->extension_items()->IsCommandIdVisible(command_id);
    }
    return false;
  }
#endif
  return true;
}

bool TabContextMenuController::IsCommandIdAlerted(int command_id) const {
  return delegate_->IsContextMenuCommandAlerted(
      static_cast<TabStripModel::ContextMenuCommand>(command_id));
}

void TabContextMenuController::ExecuteCommand(int command_id, int event_flags) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(command_id)) {
    if (tab_menu_model_ && tab_menu_model_->extension_items()) {
      content::WebContents* web_contents =
          tab_handle_.Get() ? tab_handle_.Get()->GetContents() : nullptr;
      if (web_contents) {
        content::ContextMenuParams params;
        params.page_url = web_contents->GetLastCommittedURL();
        tab_menu_model_->extension_items()->ExecuteCommand(
            command_id, web_contents, nullptr, params);
      }
    }
    return;
  }
#endif
  if (auto* tab = tab_handle_.Get()) {
    delegate_->ExecuteContextMenuCommand(
        tab, static_cast<TabStripModel::ContextMenuCommand>(command_id),
        event_flags);
  }
}

bool TabContextMenuController::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return delegate_->GetContextMenuAccelerator(command_id, accelerator);
}
