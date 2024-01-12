// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_APPS_PLUGIN_VM_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_APPS_PLUGIN_VM_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

class Profile;

namespace ash::settings {

class PluginVmHandler : public ::settings::SettingsPageUIHandler {
 public:
  explicit PluginVmHandler(Profile* profile);

  PluginVmHandler(const PluginVmHandler&) = delete;
  PluginVmHandler& operator=(const PluginVmHandler&) = delete;

  ~PluginVmHandler() override;

  // SettingsPageUIHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // Checks if Plugin VM would need to be relaunched if the proposed changes are
  // made.
  void HandleIsRelaunchNeededForNewPermissions(const base::Value::List& args);
  // Relaunches Plugin VM.
  void HandleRelaunchPluginVm(const base::Value::List& args);

  raw_ptr<Profile> profile_;
  // weak_ptr_factory_ should always be last member.
  base::WeakPtrFactory<PluginVmHandler> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_APPS_PLUGIN_VM_HANDLER_H_
