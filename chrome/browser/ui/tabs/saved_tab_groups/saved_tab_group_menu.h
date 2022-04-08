// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MENU_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MENU_H_

#include <memory>

#include "base/callback_forward.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"

class MenuButtonController;
class Widget;
struct SavedTabGroup;

// Builds the context menu for a saved tab group. The context menu consists of
// the favicon and tab title for each tab found in a saved tab group.
class SavedTabGroupMenu : public ui::SimpleMenuModel,
                          public ui::SimpleMenuModel::Delegate {
 public:
  using OpenUrlCallback =
      base::OnceCallback<void(const content::OpenURLParams&)>;

  // This SavedTabGroup is owned by the button which holds this context menu.
  explicit SavedTabGroupMenu(const SavedTabGroup* saved_group);
  SavedTabGroupMenu(const SavedTabGroupMenu&) = delete;
  SavedTabGroupMenu& operator=(const SavedTabGroupMenu&) = delete;
  ~SavedTabGroupMenu() override;

  // Displays the context menu for a saved tab group.
  void RunMenu(views::Widget* parent,
               views::MenuButtonController* button_controller,
               const gfx::Rect& bounds,
               OpenUrlCallback open_url);

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // Builds the context menu for 'saved_group'.
  void BuildMenu();

  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  const SavedTabGroup* saved_group_;
  OpenUrlCallback open_url_;
};

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MENU_H_
