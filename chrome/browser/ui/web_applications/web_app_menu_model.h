// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_MENU_MODEL_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_MENU_MODEL_H_

#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"

namespace chromeos {
class MoveToDesksMenuModel;
}  // namespace chromeos

// Menu model for the menu button in a web app browser window.
class WebAppMenuModel : public AppMenuModel {
 public:
  static constexpr int kUninstallAppCommandId = 1;
  static constexpr int kExtensionsMenuCommandId = 2;

  WebAppMenuModel(ui::AcceleratorProvider* provider, Browser* browser);
  WebAppMenuModel(const WebAppMenuModel&) = delete;
  WebAppMenuModel& operator=(const WebAppMenuModel&) = delete;
  ~WebAppMenuModel() override;

  // AppMenuModel:
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 protected:
  // AppMenuModel:
  void Build() override;

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<chromeos::MoveToDesksMenuModel> move_to_desks_submenu_;
#endif
};

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_MENU_MODEL_H_
