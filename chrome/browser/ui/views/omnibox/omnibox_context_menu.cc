// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_context_menu.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "components/favicon_base/favicon_types.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"

OmniboxContextMenu::OmniboxContextMenu(views::Widget* parent_widget,
                                       OmniboxPopupFileSelector* file_selector,
                                       content::WebContents* web_contents,
                                       base::RepeatingClosure on_menu_closed)
    : parent_widget_(parent_widget),
      controller_(std::make_unique<OmniboxContextMenuController>(file_selector,
                                                                 web_contents)),
      on_menu_closed_(std::move(on_menu_closed)) {
  std::unique_ptr<views::MenuItemView> menu =
      std::make_unique<views::MenuItemView>(this);
  menu_ = menu.get();
  menu_runner_ = std::make_unique<views::MenuRunner>(
      std::move(menu), views::MenuRunner::HAS_MNEMONICS |
                           views::MenuRunner::MENU_ITEM_CONTEXT_MENU);
  ui::SimpleMenuModel* menu_model = controller_->menu_model();
  menu_model->SetMenuModelDelegate(this);
  for (size_t i = 0; i < menu_model->GetItemCount(); ++i) {
    views::MenuModelAdapter::AppendMenuItemFromModel(
        menu_model, i, menu_, menu_model->GetCommandIdAt(i));
  }
}

int OmniboxContextMenu::GetMaxWidthForMenu(views::MenuItemView* menu) {
  return 400;
}

OmniboxContextMenu::~OmniboxContextMenu() {
  if (controller_ && controller_->menu_model()) {
    controller_->menu_model()->SetMenuModelDelegate(nullptr);
  }
}

void OmniboxContextMenu::RunMenuAt(const gfx::Point& point,
                                   ui::mojom::MenuSourceType source_type) {
  menu_runner_->RunMenuAt(parent_widget_, nullptr,
                          gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

void OmniboxContextMenu::ExecuteCommand(int command_id, int event_flags) {
  controller_->ExecuteCommand(command_id, event_flags);
}

// Needed so that titles display correctly.
const gfx::FontList* OmniboxContextMenu::GetLabelFontList(
    int command_id) const {
  ui::MenuModel* model = controller_->menu_model();
  size_t index = 0;
  ui::MenuModel::GetModelAndIndexForCommandId(command_id, &model, &index);
  return model->GetLabelFontListAt(index);
}

std::optional<SkColor> OmniboxContextMenu::GetLabelColor(int command_id) const {
  // Use STYLE_PRIMARY for title item. This aligns with 3-dot menu title style.
  return command_id == ui::MenuModel::kTitleId
             ? std::make_optional(
                   menu_->GetSubmenu()->GetColorProvider()->GetColor(
                       views::TypographyProvider::Get().GetColorId(
                           views::style::CONTEXT_MENU,
                           views::style::STYLE_PRIMARY)))
             : std::nullopt;
}

bool OmniboxContextMenu::IsCommandEnabled(int command_id) const {
  return controller_->IsCommandIdEnabled(command_id);
}

bool OmniboxContextMenu::IsCommandVisible(int command_id) const {
  return controller_->IsCommandIdVisible(command_id);
}

void OmniboxContextMenu::OnMenuClosed(views::MenuItemView* menu) {
  if (on_menu_closed_) {
    on_menu_closed_.Run();
  }
}

void OmniboxContextMenu::OnIconChanged(int command_id) {
  const std::optional<size_t> index =
      controller_->menu_model()->GetIndexOfCommandId(command_id);
  DCHECK(index.has_value());
  views::MenuItemView* menu_item =
      menu_->GetSubmenu()->GetMenuItemAt(index.value());
  if (menu_item) {
    menu_item->SetIcon(controller_->menu_model()->GetIconAt(index.value()));
  }
}
