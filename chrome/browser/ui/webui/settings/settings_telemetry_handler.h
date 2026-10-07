// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_TELEMETRY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_TELEMETRY_HANDLER_H_

#include "base/values.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace settings {

// WebUI message handler for Settings Critical User Journey (CUJ) telemetry
// events dispatched from TypeScript via MetricsBrowserProxy.
class SettingsTelemetryHandler : public SettingsPageUIHandler {
 public:
  SettingsTelemetryHandler();
  SettingsTelemetryHandler(const SettingsTelemetryHandler&) = delete;
  SettingsTelemetryHandler& operator=(const SettingsTelemetryHandler&) = delete;
  ~SettingsTelemetryHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

 private:
  void HandleNavCategoryClicked(const base::ListValue& args);
  void HandleSearchQueryEntered(const base::ListValue& args);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_TELEMETRY_HANDLER_H_
