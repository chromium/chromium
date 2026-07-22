// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/app_menu/app_menu_section_action_item.h"
#include "ui/actions/actions.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"

ActionAppMenu::ActionAppMenu(
    BrowserWindowInterface* browser_window_interface,
    std::unique_ptr<AppMenuActionManager> action_manager,
    base::RepeatingClosure on_menu_closed_callback)
    : browser_window_interface_(browser_window_interface),
      on_menu_closed_callback_(std::move(on_menu_closed_callback)),
      action_manager_(std::move(action_manager)) {}

ActionAppMenu::~ActionAppMenu() = default;

void ActionAppMenu::RunMenu(views::MenuButtonController* host) {
  auto root = std::make_unique<views::MenuItemView>(/*delegate=*/this);
  // Stash the raw pointer before transferring the unique_ptr ownership to
  // `menu_runner_`. This allows us to reference the root menu item view later.
  root_ = root.get();

  PopulateMenu(root_, action_manager_->root_action_item());

  int32_t types = views::MenuRunner::HAS_MNEMONICS;
  menu_runner_ = std::make_unique<views::MenuRunner>(std::move(root), types);

  // TODO(crbug.com/526712325): Create duplicate app menu histograms specific to
  // the Block Style ChroMenu.
  menu_runner_->RunMenuAt(host->button()->GetWidget(), host,
                          host->button()->GetAnchorBoundsInScreen(),
                          views::MenuAnchorPosition::kTopRight,
                          ui::mojom::MenuSourceType::kNone);
}

void ActionAppMenu::CloseMenu() {
  if (menu_runner_) {
    menu_runner_->Cancel();
  }
}

bool ActionAppMenu::IsShowing() const {
  return menu_runner_ && menu_runner_->IsRunning();
}

void ActionAppMenu::ExecuteCommand(int id, int mouse_event_flags) {
  auto action_iterator = command_to_action_map_.find(id);
  // Check if key exists in the map before invoking the action.
  // If the key does not exist, .find() returns command_to_action_map_.end()
  if (action_iterator != command_to_action_map_.end()) {
    action_iterator->second->InvokeAction();
  }
}

void ActionAppMenu::OnMenuClosed(views::MenuItemView* menu) {
  if (on_menu_closed_callback_) {
    on_menu_closed_callback_.Run();
  }
}

void ActionAppMenu::PopulateMenu(views::MenuItemView* view_parent,
                                 actions::ActionItem* action_item) {
  // Every item created during a for loop  will be a child of the current
  // view_parent.
  for (const auto& child : action_item->GetChildren().children()) {
    actions::ActionItem* child_ptr = child.get();
    // If the child is a header, append it as a MenuItemView that represents a
    // title.
    if (actions::IsActionItemClass<AppMenuSectionActionItem>(child_ptr)) {
      auto* title_item =
          view_parent->AppendTitle(std::u16string(child_ptr->GetText()));
      title_item->SetEnabled(false);
      // Recursive call using the same view_parent to keep the chldren in
      // the same menu section as the header.
      PopulateMenu(view_parent, child_ptr);
    } else {
      std::optional<actions::ActionId> action_id = child_ptr->GetActionId();
      CHECK(action_id.has_value());

      // Otherwise, append it as a MenuItemView that represents an action item.
      auto* menu_item = view_parent->AppendMenuItem(action_id.value());
      action_view_controller_.CreateActionViewRelationship(
          menu_item, child_ptr->GetAsWeakPtr());
      command_to_action_map_[action_id.value()] = child_ptr;

      // Recursive call using the new menu_item as the view_parent.
      PopulateMenu(menu_item, child_ptr);
    }
  }
}
