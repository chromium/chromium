// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_PRIVACY_HUB_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_PRIVACY_HUB_HANDLER_H_

#include <string>

#include "ash/public/cpp/privacy_hub_delegate.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace ash::settings {

class PrivacyHubHandler : public content::WebUIMessageHandler,
                          public PrivacyHubDelegate {
 public:
  PrivacyHubHandler();
  ~PrivacyHubHandler() override;

  PrivacyHubHandler(const PrivacyHubHandler&) = delete;

  PrivacyHubHandler& operator=(const PrivacyHubHandler&) = delete;

  // PrivacyHubDelegate
  void MicrophoneHardwareToggleChanged(bool muted) override;

 protected:
  // content::WebUIMessageHandler
  void RegisterMessages() override;

  void NotifyJS(const std::string& event_name, const base::Value& value);

  void HandleInitialMicrophoneSwitchState(const base::Value::List& args);
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_PRIVACY_HUB_HANDLER_H_
