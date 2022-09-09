// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SYSTEM_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SYSTEM_HANDLER_H_

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace content {
class WebUIDataSource;
}

namespace settings {

class SystemHandler : public SettingsPageUIHandler {
 public:
  SystemHandler();

  SystemHandler(const SystemHandler&) = delete;
  SystemHandler& operator=(const SystemHandler&) = delete;

  ~SystemHandler() override;

  // Populates handler-specific loadTimeData values used by the system page.
  static void AddLoadTimeData(content::WebUIDataSource* data_source);

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

 private:
  // Handler for the "showProxySettings" message. No args.
  void HandleShowProxySettings(const base::Value::List& args);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SYSTEM_HANDLER_H_
