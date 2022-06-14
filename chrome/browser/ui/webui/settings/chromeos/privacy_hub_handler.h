// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_PRIVACY_HUB_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_PRIVACY_HUB_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

namespace chromeos::settings {

class PrivacyHubHandler : public content::WebUIMessageHandler,
                          public media::CameraPrivacySwitchObserver {
 public:
  PrivacyHubHandler();

  PrivacyHubHandler(const PrivacyHubHandler&) = delete;

  PrivacyHubHandler& operator=(const PrivacyHubHandler&) = delete;

 private:
  // content::WebUIMessageHandler
  void RegisterMessages() override;

  // media::CameraPrivacySwitchObserver
  void OnCameraPrivacySwitchStatusChanged(
      cros::mojom::CameraPrivacySwitchState state) override;

  void HandleInitial(const base::Value::List& args);

  cros::mojom::CameraPrivacySwitchState camera_privacy_switch_state_;
};

}  // namespace chromeos::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_PRIVACY_HUB_HANDLER_H_
