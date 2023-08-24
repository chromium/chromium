// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_EXISTING_WINDOW_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_EXISTING_WINDOW_SUB_MENU_MODEL_H_

#include <stddef.h>

#include "base/types/pass_key.h"
#include "chrome/browser/ui/tabs/existing_base_sub_menu_model.h"

class Browser;
class Profile;
class TabStripModel;
class TabMenuModelDelegate;

namespace ui {
class Accelerator;
}  // namespace ui

class ExistingWindowSubMenuModel : public ExistingBaseSubMenuModel {
 public:
  // Factory function for creating a platform-specific
  // ExistingWindowSubMenuModel.
  static std::unique_ptr<ExistingWindowSubMenuModel> Create(
      ui::SimpleMenuModel::Delegate* parent_delegate,
      TabMenuModelDelegate* tab_menu_model_delegate,
      TabStripModel* model,
      int context_index);

  // Clients shouldn't directly create instances of ExistingWindowSubMenuModel,
  // but rather use ExistingWindowSubMenuModel::Create().
  ExistingWindowSubMenuModel(base::PassKey<ExistingWindowSubMenuModel> passkey,
                             ui::SimpleMenuModel::Delegate* parent_delegate,
                             TabMenuModelDelegate* tab_menu_model_delegate,
                             TabStripModel* model,
                             int context_index);
  ExistingWindowSubMenuModel(const ExistingWindowSubMenuModel&) = delete;
  ExistingWindowSubMenuModel& operator=(const ExistingWindowSubMenuModel&) =
      delete;
  ~ExistingWindowSubMenuModel() override;

  // ui::SimpleMenuModel:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;

  // Whether the submenu should be shown in the provided context. True iff
  // the submenu would show at least one window. Does not assume ownership of
  // |model|; |model| must outlive this instance.
  static bool ShouldShowSubmenu(Profile* profile);
  static bool ShouldShowSubmenuForApp(
      TabMenuModelDelegate* tab_menu_model_delegate);

 protected:
  // Retrieves a base::Passkey which can be used to construct an instance of
  // ExistingWindowSubMenuModel.
  static base::PassKey<ExistingWindowSubMenuModel> GetPassKey();

  // Builds a vector of MenuItemInfo structs for the given browsers.
  static std::vector<ExistingWindowSubMenuModel::MenuItemInfo>
  BuildMenuItemInfoVectorForBrowsers(
      const std::vector<Browser*>& existing_browsers);

 private:
  // ExistingBaseSubMenuModel:
  void ExecuteExistingCommand(size_t target_index) override;
};

#endif  // CHROME_BROWSER_UI_TABS_EXISTING_WINDOW_SUB_MENU_MODEL_H_
