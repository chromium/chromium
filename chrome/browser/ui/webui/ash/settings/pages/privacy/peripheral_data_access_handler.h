// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRIVACY_PERIPHERAL_DATA_ACCESS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRIVACY_PERIPHERAL_DATA_ACCESS_HANDLER_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

namespace ash::settings {

class PeripheralDataAccessHandler : public content::WebUIMessageHandler {
 public:
  static bool GetPrefState();

  explicit PeripheralDataAccessHandler(Profile* profile);
  ~PeripheralDataAccessHandler() override;

  PeripheralDataAccessHandler(const PeripheralDataAccessHandler&) = delete;
  PeripheralDataAccessHandler& operator=(const PeripheralDataAccessHandler&) =
      delete;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // Handles checking if thunderbolt is supported in this device.
  void HandleThunderboltSupported(const base::ListValue& args);

  // Handles returning the policy state.
  void HandleGetPolicyState(const base::ListValue& args);

  // Observer for the CrosSetting.
  void OnPeripheralDataAccessProtectionChanged();

  void OnFilePathChecked(const std::string& callback_id,
                         bool is_thunderbolt_supported);

  void OnLocalStatePrefChanged();

  base::CallbackListSubscription peripheral_data_access_subscription_;

  bool is_user_configurable_ = false;

  // Used for callbacks.
  base::WeakPtrFactory<PeripheralDataAccessHandler> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRIVACY_PERIPHERAL_DATA_ACCESS_HANDLER_H_
