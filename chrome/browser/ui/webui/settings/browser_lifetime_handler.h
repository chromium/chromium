// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_BROWSER_LIFETIME_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_BROWSER_LIFETIME_HANDLER_H_

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace settings {

class BrowserLifetimeHandler : public SettingsPageUIHandler {
 public:
  BrowserLifetimeHandler();

  BrowserLifetimeHandler(const BrowserLifetimeHandler&) = delete;
  BrowserLifetimeHandler& operator=(const BrowserLifetimeHandler&) = delete;

  ~BrowserLifetimeHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

 private:
  void HandleRestart(const base::Value::List& args);
  void HandleRelaunch(const base::Value::List& args);
#if BUILDFLAG(IS_CHROMEOS)
  void HandleSignOutAndRestart(const base::Value::List& args);
  void HandleFactoryReset(const base::Value::List& args);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS)
  void HandleGetRelaunchConfirmationDialogDescription(
      const base::Value::List& args);
  void HandleShouldShowRelaunchConfirmationDialog(
      const base::Value::List& args);
#endif
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_BROWSER_LIFETIME_HANDLER_H_
