// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_PRIVACY_HUB_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_PRIVACY_HUB_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "ui/events/devices/microphone_mute_switch_monitor.h"

namespace chromeos::settings {

class PrivacyHubHandler : public content::WebUIMessageHandler,
                          public media::CameraPrivacySwitchObserver,
                          public ui::MicrophoneMuteSwitchMonitor::Observer {
 public:
  PrivacyHubHandler();
  ~PrivacyHubHandler() override;

  PrivacyHubHandler(const PrivacyHubHandler&) = delete;

  PrivacyHubHandler& operator=(const PrivacyHubHandler&) = delete;

 protected:
  // content::WebUIMessageHandler
  void RegisterMessages() override;

  // media::CameraPrivacySwitchObserver
  void OnCameraHWPrivacySwitchStatusChanged(
      int32_t camera_id,
      cros::mojom::CameraPrivacySwitchState state) override;

  // ui::MicrophoneMuteSwitchMonitor::Observer
  void OnMicrophoneMuteSwitchValueChanged(bool muted) override;

  void HandleInitialCameraSwitchState(const base::Value::List& args);

  void HandleInitialMicrophoneSwitchState(const base::Value::List& args);

 private:
  cros::mojom::CameraPrivacySwitchState camera_privacy_switch_state_;
};

}  // namespace chromeos::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_PRIVACY_HUB_HANDLER_H_
