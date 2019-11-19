// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_TERMINAL_SYSTEM_APP_MENU_MODEL_CHROMEOS_H_
#define CHROME_BROWSER_UI_EXTENSIONS_TERMINAL_SYSTEM_APP_MENU_MODEL_CHROMEOS_H_

#include "base/macros.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"

// Menu model for the Terminal System App menu button.
class TerminalSystemAppMenuModel : public AppMenuModel {
 public:
  TerminalSystemAppMenuModel(ui::AcceleratorProvider* provider,
                             Browser* browser);
  ~TerminalSystemAppMenuModel() override;

 private:
  // AppMenuModel:
  void Build() override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void LogMenuAction(AppMenuAction action_id) override;

  DISALLOW_COPY_AND_ASSIGN(TerminalSystemAppMenuModel);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_TERMINAL_SYSTEM_APP_MENU_MODEL_CHROMEOS_H_
