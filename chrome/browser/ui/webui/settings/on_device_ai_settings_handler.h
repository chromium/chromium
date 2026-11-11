// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ON_DEVICE_AI_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ON_DEVICE_AI_SETTINGS_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/prefs/pref_member.h"

namespace features {
BASE_DECLARE_FEATURE(kOnDeviceModelAdvancedSettings);
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

  // Handles the "openOnDeviceAiFeedbackDialog" message from the UI. Protected
  // for testing.
  void HandleOpenFeedbackDialog(const base::ListValue& args);

  // Handles the "getOnDeviceAiCapabilities" message from the UI. Protected for
  // testing.
  void HandleGetOnDeviceAiCapabilities(const base::ListValue& args);

  // Handles the "deleteOnDeviceAiCapability" message from the UI. Protected for
  // testing.
  void HandleDeleteOnDeviceAiCapability(const base::ListValue& args);

  // Returns the OnDeviceModelControl instance, or nullptr if unavailable.
  // Virtual for testing.
  virtual optimization_guide::OnDeviceModelControl* GetModelControl();

 private:
  // Returns the current state of the on-device AI setting.
  base::DictValue GetOnDeviceAiState();

  // Queries installed models from OnDeviceModelControl and resolves
  // `callback_id` with the formatted capabilities dictionary.
  void QueryAndResolveCapabilities(const std::string& callback_id);

  // Callback invoked when installed models and their disk sizes are loaded.
  void OnCapabilitiesLoaded(
      const std::string& callback_id,
      std::vector<optimization_guide::OnDeviceModelControl::InstalledModel>
          models);

  // Called when the kOnDeviceAiEnabled preference changes.
  void OnPrefChange();

  // Sends a "on-device-ai-enabled-change" WebUI listener event to the page.
  void SendOnDeviceAiEnabledChange();

  // Used to track pref changes that affect whether on device AI is enabled.
  std::unique_ptr<BooleanPrefMember> pref_member_;

  base::WeakPtrFactory<OnDeviceAiSettingsHandler> weak_ptr_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ON_DEVICE_AI_SETTINGS_HANDLER_H_
