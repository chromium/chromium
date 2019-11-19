// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_INTERNAL_APP_INTERNAL_APP_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_APP_LIST_INTERNAL_APP_INTERNAL_APP_CONTEXT_MENU_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_context_menu.h"

class AppListControllerDelegate;
class Profile;

class InternalAppContextMenu : public app_list::AppContextMenu {
 public:
  InternalAppContextMenu(Profile* profile,
                         const std::string& app_id,
                         AppListControllerDelegate* controller);
  ~InternalAppContextMenu() override;

  // app_list::AppContextMenu:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void BuildMenu(ui::SimpleMenuModel* menu_model) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InternalAppContextMenu);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_INTERNAL_APP_INTERNAL_APP_CONTEXT_MENU_H_
