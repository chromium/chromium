// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_MAC_SYSTEM_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_MAC_SYSTEM_SETTINGS_HANDLER_H_

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace settings {

class MacSystemSettingsHandler : public SettingsPageUIHandler {
 public:
  MacSystemSettingsHandler();

  MacSystemSettingsHandler(const MacSystemSettingsHandler&) = delete;
  MacSystemSettingsHandler& operator=(const MacSystemSettingsHandler&) = delete;

  ~MacSystemSettingsHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

 private:
  void HandleOpenTrackpadGesturesSettings(const base::Value::List& args);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_MAC_SYSTEM_SETTINGS_HANDLER_H_
