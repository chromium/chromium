// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_ACTION_APP_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_ACTION_APP_MENU_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/controls/menu/menu_delegate.h"

class BrowserWindowInterface;

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
  void OnMenuClosed(views::MenuItemView* menu) override;

 private:
  // The browser window interface associated with this menu.
  raw_ptr<BrowserWindowInterface> browser_window_interface_;

  // Callback run when the menu is closed to notify the menu button.
  base::RepeatingClosure on_menu_closed_callback_;

  // Manages the widget and popup execution lifecycle of the menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // The root menu item view. Owned by `menu_runner_`.
  raw_ptr<views::MenuItemView> root_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_ACTION_APP_MENU_H_
