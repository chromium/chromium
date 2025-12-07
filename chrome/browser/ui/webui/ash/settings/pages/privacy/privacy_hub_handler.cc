// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/privacy/privacy_hub_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/synchronization/condition_variable.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/prefs/pref_service.h"

namespace ash::settings {

PrivacyHubHandler::PrivacyHubHandler()
    : mic_muted_by_security_curtain_(
          CrasAudioHandler::Get()->IsInputMutedBySecurityCurtain()) {
  this_account_id_ = Shell::Get()->session_controller()->GetActiveAccountId();
}

PrivacyHubHandler::~PrivacyHubHandler() {
  privacy_hub_util::SetFrontend(nullptr);
}

void PrivacyHubHandler::RegisterMessages() {
  privacy_hub_util::SetFrontend(this);
  web_ui()->RegisterMessageCallback(
      "getInitialMicrophoneHardwareToggleState",
      base::BindRepeating(
          &PrivacyHubHandler::HandleInitialMicrophoneSwitchState,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getInitialMicrophoneMutedBySecurityCurtainState",
      base::BindRepeating(
          &PrivacyHubHandler::
              HandleInitialMicrophoneMutedBySecurityCurtainState,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getInitialCameraSwitchForceDisabledState",
      base::BindRepeating(
          &PrivacyHubHandler::HandleInitialCameraSwitchForceDisabledState,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getInitialGeolocationAccessLevelState",
      base::BindRepeating(
          &PrivacyHubHandler::HandleInitialPrimaryUserLocationState,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCameraLedFallbackState",
      base::BindRepeating(
          &PrivacyHubHandler::HandleInitialCameraLedFallbackState,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCurrentTimeZoneName",
      base::BindRepeating(&PrivacyHubHandler::HandleGetCurrentTimezoneName,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCurrentSunriseTime",
      base::BindRepeating(&PrivacyHubHandler::HandleGetCurrentSunRiseTime,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCurrentSunsetTime",
      base::BindRepeating(&PrivacyHubHandler::HandleGetCurrentSunSetTime,
                          base::Unretained(this)));
}

void PrivacyHubHandler::NotifyJS(const std::string& event_name,
                                 const base::Value& value) {
  if (IsJavascriptAllowed()) {
    FireWebUIListener(event_name, value);
  } else {
    DVLOG(1) << "JS disabled. Skip \"" << event_name
             << "\" event until enabled.";
  }
}

void PrivacyHubHandler::MicrophoneHardwareToggleChanged(bool muted) {
  NotifyJS("microphone-hardware-toggle-changed", base::Value(muted));
}

void PrivacyHubHandler::SetForceDisableCameraSwitch(bool disabled) {
  NotifyJS("force-disable-camera-switch", base::Value(disabled));
}

void PrivacyHubHandler::SystemGeolocationAccessLevelChanged(
    GeolocationAccessLevel access_level) {
  NotifyJS("system-geolocation-access-level-changed",
           base::Value(static_cast<int>(access_level)));
}

void PrivacyHubHandler::OnInputMutedBySecurityCurtainChanged(bool muted) {
  if (mic_muted_by_security_curtain_ == muted) {
    return;
  }
  mic_muted_by_security_curtain_ = muted;

  NotifyJS("microphone-muted-by-security-curtain-changed", base::Value(muted));
}
void PrivacyHubHandler::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  // If the user associated to this instance has become active, mark this
  // instance as the active frontend.
  if (account_id == this_account_id_) {
    privacy_hub_util::SetFrontend(this);
  }
}

void PrivacyHubHandler::HandleInitialMicrophoneSwitchState(
    const base::Value::List& args) {
  const auto callback_id = ValidateArgs(args);
  const auto value = base::Value(privacy_hub_util::MicrophoneSwitchState());
  ResolveJavascriptCallback(callback_id, value);
}

void PrivacyHubHandler::HandleInitialMicrophoneMutedBySecurityCurtainState(
    const base::Value::List& args) {
  const auto callback_id = ValidateArgs(args);
  const auto value = base::Value(mic_muted_by_security_curtain_);
  ResolveJavascriptCallback(callback_id, value);
}

void PrivacyHubHandler::HandleInitialCameraSwitchForceDisabledState(
    const base::Value::List& args) {
  const auto callback_id = ValidateArgs(args);
  const auto is_disabled =
      base::Value(privacy_hub_util::ShouldForceDisableCameraSwitch());
  ResolveJavascriptCallback(callback_id, is_disabled);
}

void PrivacyHubHandler::HandleInitialCameraLedFallbackState(
    const base::Value::List& args) {
  const auto callback_id = ValidateArgs(args);
  const auto value = base::Value(privacy_hub_util::UsingCameraLEDFallback());
  ResolveJavascriptCallback(callback_id, value);
}

void PrivacyHubHandler::HandleInitialPrimaryUserLocationState(
    const base::Value::List& args) {
  const auto callback_id = ValidateArgs(args);
  const auto value = base::Value(
      static_cast<int>(privacy_hub_util::GetSystemGeolocationAccessLevel()));
  ResolveJavascriptCallback(callback_id, value);
}

void PrivacyHubHandler::HandleGetCurrentTimezoneName(
    const base::Value::List& args) {
  const auto callback_id = ValidateArgs(args);
  const auto value = base::Value(system::GetCurrentTimezoneName());
  ResolveJavascriptCallback(callback_id, value);
}

void PrivacyHubHandler::HandleGetCurrentSunRiseTime(
    const base::Value::List& args) {
  const auto callback_id = ValidateArgs(args);
  const auto value = base::Value(base::TimeFormatTimeOfDay(
      ash::privacy_hub_util::SunriseSunsetSchedule().first));
  ResolveJavascriptCallback(callback_id, value);
}

void PrivacyHubHandler::HandleGetCurrentSunSetTime(
    const base::Value::List& args) {
  const auto callback_id = ValidateArgs(args);
  const auto value = base::Value(base::TimeFormatTimeOfDay(
      ash::privacy_hub_util::SunriseSunsetSchedule().second));
  ResolveJavascriptCallback(callback_id, value);
}

const base::ValueView PrivacyHubHandler::ValidateArgs(
    const base::Value::List& args) {
  // TODO(b/290646585): Replace with a CHECK().
  AllowJavascript();

  DCHECK_GE(1U, args.size()) << ": Did not expect arguments";
  DCHECK_EQ(1U, args.size()) << ": Callback ID is required";

  return args[0];
}

}  // namespace ash::settings
