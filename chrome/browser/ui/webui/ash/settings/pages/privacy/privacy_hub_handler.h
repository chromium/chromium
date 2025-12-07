// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRIVACY_PRIVACY_HUB_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRIVACY_PRIVACY_HUB_HANDLER_H_

#include <optional>
#include <string>

#include "ash/public/cpp/privacy_hub_delegate.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/values.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace ash::settings {

class PrivacyHubHandler : public content::WebUIMessageHandler,
                          public CrasAudioHandler::AudioObserver,
                          public PrivacyHubDelegate,
                          public SessionObserver {
 public:
  PrivacyHubHandler();
  ~PrivacyHubHandler() override;

  PrivacyHubHandler(const PrivacyHubHandler&) = delete;

  PrivacyHubHandler& operator=(const PrivacyHubHandler&) = delete;

  // PrivacyHubDelegate
  void MicrophoneHardwareToggleChanged(bool muted) override;
  void SetForceDisableCameraSwitch(bool disabled) override;
  void SystemGeolocationAccessLevelChanged(
      GeolocationAccessLevel access_level) override;

  // CrasAudioHandler::AudioObserver
  void OnInputMutedBySecurityCurtainChanged(bool muted) override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

 protected:
  // content::WebUIMessageHandler
  void RegisterMessages() override;

  void NotifyJS(const std::string& event_name, const base::Value& value);

  void HandleInitialMicrophoneSwitchState(const base::Value::List& args);
  void HandleInitialMicrophoneMutedBySecurityCurtainState(
      const base::Value::List& args);
  void HandleInitialCameraSwitchForceDisabledState(
      const base::Value::List& args);
  void HandleInitialCameraLedFallbackState(const base::Value::List& args);
  void HandleInitialPrimaryUserLocationState(const base::Value::List& args);
  void HandleGetCurrentTimezoneName(const base::Value::List& args);
  void HandleGetCurrentSunSetTime(const base::Value::List& args);
  void HandleGetCurrentSunRiseTime(const base::Value::List& args);

 private:
  ScopedSessionObserver session_observer_{this};

  // return the callback_id
  const base::ValueView ValidateArgs(const base::Value::List& args);

  bool mic_muted_by_security_curtain_ = false;

  AccountId this_account_id_ = EmptyAccountId();

  base::WeakPtrFactory<PrivacyHubHandler> weak_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRIVACY_PRIVACY_HUB_HANDLER_H_
