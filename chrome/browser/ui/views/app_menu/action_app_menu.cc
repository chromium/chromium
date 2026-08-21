// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/browser/ui/views/app_menu/app_menu_section_action_item.h"
#include "chrome/browser/ui/views/app_menu/bookmarks_dynamic_menu.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/actions/actions.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

ActionAppMenu::ActionAppMenu(BrowserWindowInterface* browser_window_interface,
                             base::RepeatingClosure on_menu_closed_callback)
    : browser_window_interface_(browser_window_interface),
      on_menu_closed_callback_(std::move(on_menu_closed_callback)),
      menu_manager_(
          std::make_unique<ActionAppMenuManager>(browser_window_interface)) {
  menu_manager_->CreateMenuHierarchy();
}

ActionAppMenu::~ActionAppMenu() {
  command_to_action_map_.clear();
  menu_manager_->GetAppMenuRoot()->ResetActionList();
}

void ActionAppMenu::RunMenu(views::MenuButtonController* host) {
  auto root = std::make_unique<views::MenuItemView>(/*delegate=*/this);
  // Stash the raw pointer before transferring the unique_ptr ownership to
  // `menu_runner_`. This allows us to reference the root menu item view later.
  root_ = root.get();

  const auto* provider = ChromeLayoutProvider::Get();
  PopulateMenu(root_, menu_manager_->GetAppMenuRoot());

  root_->set_children_use_full_width(true);
  views::SubmenuView* submenu = root_->CreateSubmenu();
  submenu->SetBorder(views::CreateEmptyBorder(
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_POPUP)));
  submenu->set_minimum_preferred_width(
      provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_MINIMUM_WIDTH));

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
  command_to_action_map_.clear();
  if (on_menu_closed_callback_) {
    on_menu_closed_callback_.Run();
  }
  menu_manager_->GetAppMenuRoot()->ResetActionList();
}

const gfx::FontList* ActionAppMenu::GetLabelFontList(int id) const {
  if (id == ui::MenuModel::kTitleId) {
    return &views::TypographyProvider::Get().GetFont(
        views::style::CONTEXT_LABEL, views::style::STYLE_HEADLINE_5);
  }
  return nullptr;
}

std::optional<SkColor> ActionAppMenu::GetLabelColor(int id) const {
  if (id == ui::MenuModel::kTitleId && root_ && root_->HasSubmenu() &&
      root_->GetSubmenu()->GetColorProvider()) {
    return root_->GetSubmenu()->GetColorProvider()->GetColor(
        ui::kColorMenuItemForeground);
  }
  return std::nullopt;
}

void ActionAppMenu::PopulateMenu(views::MenuItemView* view_parent,
                                 actions::BaseAction* action_item) {
  const auto& children = action_item->GetChildren().children();
  const size_t child_count = children.size();

  const auto* provider = ChromeLayoutProvider::Get();

  for (size_t i = 0; i < child_count; ++i) {
    actions::BaseAction* child_base = children[i].get();
    actions::ActionItem* child_ptr = child_base->GetActionItem();
    if (!child_ptr) {
      continue;
    }

    // If the child is a section action item, append it as a MenuItem that
    // represents a section header.
    if (actions::IsActionClass<AppMenuSectionActionItem>(child_ptr)) {
      auto* header_menu_item =
          view_parent->AppendTitle(std::u16string(child_ptr->GetText()));
      header_menu_item->SetEnabled(false);
      const int vertical_margin =
          views::LayoutProvider::Get()->GetDistanceMetric(
              views::DistanceMetric::DISTANCE_RELATED_CONTROL_VERTICAL);
      header_menu_item->set_vertical_margin(vertical_margin);
      // Recursive call using the same parent to keep the children in
      // the same menu section as the header.
      PopulateMenu(view_parent, child_base);
    } else {
      // Otherwise, append it as a MenuItemView that represents an action item.
      std::optional<actions::ActionId> action_id = child_ptr->GetActionId();
      int command_id = action_id.value_or(next_id_++);

      const bool has_children = !child_base->GetChildren().children().empty();

      auto* menu_item =
          has_children ? view_parent->AppendSubMenu(
                             command_id, std::u16string(child_ptr->GetText()))
                       : view_parent->AppendMenuItem(command_id);
      action_view_controller_.CreateActionViewRelationship(
          menu_item, child_ptr->GetAsWeakPtr());
      command_to_action_map_[command_id] = child_ptr;

      if (std::u16string* text_override = children[i]->GetProperty(
              ActionAppMenuManager::kTextOverrideKey)) {
        menu_item->SetTitle(*text_override);
      }

      if (ui::ImageModel* icon_override = children[i]->GetProperty(
              ActionAppMenuManager::kIconOverrideKey)) {
        menu_item->SetIcon(*icon_override);
      }

      // Set the border radius depending on the position a menu item has in
      // its section.
      int top_radius =
          (i == 0) ? provider->GetDistanceMetric(
                         DISTANCE_ACTION_APP_MENU_CONTAINER_CORNER_RADIUS)
                   : 0;
      int top_padding =
          (i == 0) ? provider->GetDistanceMetric(
                         DISTANCE_ACTION_APP_MENU_ITEM_FIRST_TOP_PADDING)
                   : provider->GetDistanceMetric(
                         DISTANCE_ACTION_APP_MENU_ITEM_DEFAULT_VERTICAL_MARGIN);

      int bottom_radius =
          (i == child_count - 1)
              ? provider->GetDistanceMetric(
                    DISTANCE_ACTION_APP_MENU_CONTAINER_CORNER_RADIUS)
              : 0;

      int bottom_padding =
          (i == child_count - 1)
              ? provider->GetDistanceMetric(
                    DISTANCE_ACTION_APP_MENU_ITEM_LAST_BOTTOM_PADDING)
              : provider->GetDistanceMetric(
                    DISTANCE_ACTION_APP_MENU_ITEM_DEFAULT_VERTICAL_MARGIN);

      menu_item->SetBorder(views::CreateEmptyBorder(
          provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_ITEM)));

      // Display shortcut text if the ActionItem has one.
      ui::Accelerator accel = child_ptr->GetAccelerator();
      if (accel.key_code() != ui::VKEY_UNKNOWN) {
        menu_item->SetMinorText(accel.GetShortcutText());
      }

      // Get the styling from the ActionItem and apply it to its menu item.
      const ui::ColorId container_color =
          child_ptr->GetProperty(ActionAppMenuManager::kContainerColorKey);
      if (container_color != ui::kColorMenuBackground) {
        menu_item->SetContainerStyle(container_color, top_radius, bottom_radius,
                                     top_padding, bottom_padding);
        // Apply darker hover selection states matching section theme.
        menu_item->SetSelectedColorId(ui::kColorSysStateHoverOnSubtle);
      }

      // Recursively populate the menu item with the ActionItem's children.
      // This creates any submenu items.
      PopulateMenu(menu_item, child_base);
    }
  }
}
