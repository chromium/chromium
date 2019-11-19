// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_MENU_MODEL_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_MENU_MODEL_H_

#include "base/macros.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"

// Menu model for the menu button in a hosted app browser window.
class WebAppMenuModel : public AppMenuModel {
 public:
  static constexpr int kUninstallAppCommandId = 1;

  WebAppMenuModel(ui::AcceleratorProvider* provider, Browser* browser);
  ~WebAppMenuModel() override;

 private:
  // AppMenuModel:
  void Build() override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void LogMenuAction(AppMenuAction action_id) override;

  DISALLOW_COPY_AND_ASSIGN(WebAppMenuModel);
};

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_MENU_MODEL_H_
