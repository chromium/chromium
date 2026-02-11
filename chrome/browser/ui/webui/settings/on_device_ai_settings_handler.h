// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ON_DEVICE_AI_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ON_DEVICE_AI_SETTINGS_HANDLER_H_

#include "base/feature_list.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_member.h"

namespace features {
BASE_DECLARE_FEATURE(kShowOnDeviceAiSettings);
}

namespace settings {

// Settings handler for the On-Device AI settings.
class OnDeviceAiSettingsHandler : public SettingsPageUIHandler {
 public:
  OnDeviceAiSettingsHandler();
  OnDeviceAiSettingsHandler(const OnDeviceAiSettingsHandler&) = delete;
  OnDeviceAiSettingsHandler& operator=(const OnDeviceAiSettingsHandler&) =
      delete;

  ~OnDeviceAiSettingsHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 protected:
  // Handles the "getOnDeviceAiEnabled" message from the UI. Protected for
  // testing.
  void HandleGetOnDeviceAiEnabled(const base::ListValue& args);

  // Handles the "setOnDeviceAiEnabled" message from the UI. Protected for
  // testing.
  void HandleSetOnDeviceAiEnabled(const base::ListValue& args);

 private:
  // Returns the current state of the on-device AI setting.
  base::DictValue GetOnDeviceAiState();

  // Called when the kOnDeviceAiEnabled preference changes.
  void OnPrefChange();

  // Sends a "on-device-ai-enabled-change" WebUI listener event to the page.
  void SendOnDeviceAiEnabledChange();

  // Used to track pref changes that affect whether on device AI is enabled.
  std::unique_ptr<BooleanPrefMember> pref_member_;
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ON_DEVICE_AI_SETTINGS_HANDLER_H_
