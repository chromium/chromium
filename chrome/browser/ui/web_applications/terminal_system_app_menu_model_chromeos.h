// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_TERMINAL_SYSTEM_APP_MENU_MODEL_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_TERMINAL_SYSTEM_APP_MENU_MODEL_CHROMEOS_H_

#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "url/gurl.h"

// Menu model for the Terminal System App menu button.
class TerminalSystemAppMenuModel : public AppMenuModel {
 public:
  TerminalSystemAppMenuModel(ui::AcceleratorProvider* provider,
                             Browser* browser);
  TerminalSystemAppMenuModel(const TerminalSystemAppMenuModel&) = delete;
  TerminalSystemAppMenuModel& operator=(const TerminalSystemAppMenuModel&) =
      delete;
  ~TerminalSystemAppMenuModel() override;

  // Public for testing.
  // Get URL for specified menu command, returns invalid GURL for unknown.
  GURL GetURLForCommand(int command_id) const;

 private:
  // AppMenuModel:
  void Build() override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void LogMenuAction(AppMenuAction action_id) override;

  std::vector<GURL> urls_;
};

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_TERMINAL_SYSTEM_APP_MENU_MODEL_CHROMEOS_H_
