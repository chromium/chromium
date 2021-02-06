// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_SYSTEM_MENU_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_SYSTEM_MENU_MODEL_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/browser.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/simple_menu_model.h"

// Provides the SimpleMenuModel::Delegate implementation for system context
// menus.
class SystemMenuModelDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  SystemMenuModelDelegate(ui::AcceleratorProvider* provider, Browser* browser);
  ~SystemMenuModelDelegate() override;

  Browser* browser() { return browser_; }

  // Overridden from ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  bool IsItemForCommandIdDynamic(int command_id) const override;
  base::string16 GetLabelForCommandId(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  ui::AcceleratorProvider* const provider_;  // weak
  Browser* const browser_;                   // weak

  DISALLOW_COPY_AND_ASSIGN(SystemMenuModelDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_SYSTEM_MENU_MODEL_DELEGATE_H_
