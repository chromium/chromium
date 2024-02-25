// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_MAIN_OS_SETTINGS_HATS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_MAIN_OS_SETTINGS_HATS_HANDLER_H_

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace ash::settings {

// WebUI message handler for os settings HaTS.
class OsSettingsHatsHandler : public ::settings::SettingsPageUIHandler {
 public:
  explicit OsSettingsHatsHandler(Profile* profile);
  ~OsSettingsHatsHandler() override = default;

  OsSettingsHatsHandler(const OsSettingsHatsHandler&) = delete;
  OsSettingsHatsHandler& operator=(const OsSettingsHatsHandler&) = delete;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void HandleSettingsUsedSearch(const base::Value::List& args);
  void HandleSendSettingsHats(const base::Value::List& args);
  raw_ptr<Profile> profile_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_MAIN_OS_SETTINGS_HATS_HANDLER_H_
