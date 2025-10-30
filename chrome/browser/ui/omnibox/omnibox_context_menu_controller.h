// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTEXT_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTEXT_MENU_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "ui/menus/simple_menu_model.h"

class BrowserWindowInterface;

// OmniboxContextMenuController creates and manages state for the context menu
// shown for the omnibox.
class OmniboxContextMenuController : public ui::SimpleMenuModel::Delegate {
 public:
  explicit OmniboxContextMenuController(
      BrowserWindowInterface* browser_window_interface);

  OmniboxContextMenuController(const OmniboxContextMenuController&) = delete;
  OmniboxContextMenuController& operator=(const OmniboxContextMenuController&) =
      delete;

  ~OmniboxContextMenuController() override;

  ui::SimpleMenuModel* menu_model() { return menu_model_.get(); }

  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  void BuildMenu();

  // Adds a IDC_* style command to the menu with a string16.
  void AddItem(int id, const std::u16string str);
  // Adds a IDC_* style command to the menu with a localized string.
  void AddItem(int id, int localization_id);
  // Adds a separator to the menu.
  void AddSeparator();
  // Adds recent tabs as items to the menu.
  void AddRecentTabItems();

  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  raw_ptr<BrowserWindowInterface> browser_window_interface_;
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTEXT_MENU_CONTROLLER_H_
