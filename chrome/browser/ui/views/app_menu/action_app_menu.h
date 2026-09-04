// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/command_id_constants.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/menu/menu_delegate.h"

class ActionAppMenuManager;
class ActionAppMenuSearchBarView;
class BrowserWindowInterface;

namespace actions {
class ActionItem;
class BaseAction;
}  // namespace actions

namespace views {
class MenuButtonController;
class MenuItemView;
class MenuRunner;
}  // namespace views

// Coordinator class for the Block Style ChroMenu.
class ActionAppMenu : public views::MenuDelegate {
 public:
  ActionAppMenu(BrowserWindowInterface* browser_window_interface,
                base::RepeatingClosure on_menu_closed_callback);
  ActionAppMenu(const ActionAppMenu&) = delete;
  ActionAppMenu& operator=(const ActionAppMenu&) = delete;
  ~ActionAppMenu() override;

  void RunMenu(views::MenuButtonController* host);
  void CloseMenu();
  bool IsShowing() const;

  // views::MenuDelegate:
  void ExecuteCommand(int id, int mouse_event_flags) override;
  void OnMenuClosed(views::MenuItemView* menu) override;
  const gfx::FontList* GetLabelFontList(int id) const override;
  std::optional<SkColor> GetLabelColor(int id) const override;

  views::MenuItemView* root_menu_item_for_testing() { return root_; }
  ActionAppMenuSearchBarView* search_bar_for_testing() { return search_bar_; }

 private:
  // Recursively populates the menu item with the `base_action_item`'s
  // children.
  void PopulateMenu(views::MenuItemView* view_parent,
                    actions::BaseAction* base_action_item);

  // Appends and returns a menu item to the `parent_menu_item` and adds the
  // `base_action_item` to the command to action map.
  views::MenuItemView* AppendMenuItem(actions::BaseAction* base_action_item,
                                      views::MenuItemView* parent_menu_item);

  // Configures the section header in a menu to display the correct text. A
  // section header is essentially a non-interactive piece of text within the
  // menu to helps break up the menu into sections.
  void ConfigureSectionHeader(views::MenuItemView* header_menu_item);

  // Configures the menu item to populate with the correct icon, text, and
  // padding. ConfigureMenuItem() differs from ConfigureSectionHeader() in that
  // ConfigureMenuItem() should only be used for clickable menu items within the
  // action app menu or have a sub-menu.
  void ConfigureMenuItem(views::MenuItemView* menu_item,
                         actions::BaseAction* child_base,
                         bool is_first_item,
                         bool is_last_item);

  void PopulateSearchBar(views::MenuItemView* view_parent,
                         actions::ActionItem* search_action_item);

  void PopulateFooter(views::MenuItemView* view_parent,
                      actions::ActionItem* footer_action_item);

  void PopulateBlockMenuItem(views::MenuItemView* view_parent,
                             actions::ActionItem* block_action_item);

  // The browser window interface associated with this menu.
  raw_ptr<BrowserWindowInterface> browser_window_interface_;

  // Callback run when the menu is closed to notify the menu button.
  base::RepeatingClosure on_menu_closed_callback_;

  // Maps command/menu item IDs back to their corresponding ActionItem.
  base::flat_map<int, raw_ptr<actions::ActionItem>> command_to_action_map_;

  // Manages ActionItem and MenuItemView relationships.
  views::ActionViewController action_view_controller_;

  // Manages the widget and popup execution lifecycle of the menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // The root menu item view. Owned by `menu_runner_`.
  raw_ptr<views::MenuItemView> root_ = nullptr;

  // The search bar view in the menu, if kChroMenuSearch is enabled.
  raw_ptr<ActionAppMenuSearchBarView> search_bar_ = nullptr;

  // Manages the ActionItem hierarchy and dynamic submenus.
  std::unique_ptr<ActionAppMenuManager> menu_manager_;

  int next_id_ = COMMAND_ID_FIRST_UNBOUNDED;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_H_
