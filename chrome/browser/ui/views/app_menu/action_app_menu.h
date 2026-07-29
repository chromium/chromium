// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/app_menu/app_menu_action_manager.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/menu/menu_delegate.h"

class BrowserWindowInterface;

namespace actions {
class ActionItem;
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
                std::unique_ptr<AppMenuActionManager> action_manager,
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

 private:
  void PopulateMenu(views::MenuItemView* view_parent,
                    actions::ActionItem* action_item);

  // The browser window interface associated with this menu.
  raw_ptr<BrowserWindowInterface> browser_window_interface_;

  // Callback run when the menu is closed to notify the menu button.
  base::RepeatingClosure on_menu_closed_callback_;

  // Manager for the menu ActionItem tree hierarchy.
  std::unique_ptr<AppMenuActionManager> action_manager_;

  // Maps command/menu item IDs back to their corresponding ActionItem.
  base::flat_map<int, raw_ptr<actions::ActionItem>> command_to_action_map_;

  // Manages ActionItem and MenuItemView relationships.
  views::ActionViewController action_view_controller_;

  // Manages the widget and popup execution lifecycle of the menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // The root menu item view. Owned by `menu_runner_`.
  raw_ptr<views::MenuItemView> root_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_H_
