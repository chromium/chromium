// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_EXISTING_WINDOW_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_EXISTING_WINDOW_SUB_MENU_MODEL_H_

#include <stddef.h>

#include "base/macros.h"
#include "chrome/browser/ui/tabs/existing_base_sub_menu_model.h"

class Profile;
class TabStripModel;
class TabMenuModelDelegate;

class ExistingWindowSubMenuModel : public ExistingBaseSubMenuModel {
 public:
  ExistingWindowSubMenuModel(ui::SimpleMenuModel::Delegate* parent_delegate,
                             TabMenuModelDelegate* tab_menu_model_delegate,
                             TabStripModel* model,
                             int context_index);
  ExistingWindowSubMenuModel(const ExistingWindowSubMenuModel&) = delete;
  ExistingWindowSubMenuModel& operator=(const ExistingWindowSubMenuModel&) =
      delete;
  ~ExistingWindowSubMenuModel() override;

  // ui::SimpleMenuModel
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  // ui::SimpleMenuModel::Delegate
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;

  // Whether the submenu should be shown in the provided context. True iff
  // the submenu would show at least one window. Does not assume ownership of
  // |model|; |model| must outlive this instance.
  static bool ShouldShowSubmenu(Profile* profile);

 private:
  // ExistingBaseSubMenuModel
  void ExecuteNewCommand(int event_flags) override;
  void ExecuteExistingCommand(int command_index) override;
};

#endif  // CHROME_BROWSER_UI_TABS_EXISTING_WINDOW_SUB_MENU_MODEL_H_
