// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_CONTEXT_MENU_H_

#include <memory>
#include <string>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/callback.h"
#include "base/macros.h"
#include "ui/base/models/simple_menu_model.h"

class AppListControllerDelegate;
class Profile;

namespace app_list {

class AppContextMenuDelegate;

// Base class of all context menus in app list view.
class AppContextMenu : public ui::SimpleMenuModel::Delegate {
 public:
  AppContextMenu(AppContextMenuDelegate* delegate,
                 Profile* profile,
                 const std::string& app_id,
                 AppListControllerDelegate* controller);
  ~AppContextMenu() override;

  using GetMenuModelCallback =
      base::OnceCallback<void(std::unique_ptr<ui::SimpleMenuModel>)>;
  virtual void GetMenuModel(GetMenuModelCallback callback);

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsItemForCommandIdDynamic(int command_id) const override;
  base::string16 GetLabelForCommandId(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  const gfx::VectorIcon* GetVectorIconForCommandId(
      int command_id) const override;

 protected:
  // Creates default items, derived class may override to add their specific
  // items.
  virtual void BuildMenu(ui::SimpleMenuModel* menu_model);

  // Helper that toggles pinning state of provided app.
  void TogglePin(const std::string& shelf_app_id);

  // Helper method to add touchable or normal context menu options.
  void AddContextMenuOption(ui::SimpleMenuModel* menu_model,
                            ash::CommandId command_id,
                            int string_id);

  // Helper method to get the gfx::VectorIcon for a |command_id|. Returns an
  // empty gfx::VectorIcon if there is no icon for this |command_id|.
  const gfx::VectorIcon& GetMenuItemVectorIcon(int command_id,
                                               int string_id) const;

  const std::string& app_id() const { return app_id_; }
  Profile* profile() const { return profile_; }
  AppContextMenuDelegate* delegate() const { return delegate_; }
  AppListControllerDelegate* controller() const { return controller_; }

 private:
  AppContextMenuDelegate* delegate_;
  Profile* profile_;
  const std::string app_id_;
  AppListControllerDelegate* controller_;

  DISALLOW_COPY_AND_ASSIGN(AppContextMenu);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_CONTEXT_MENU_H_
