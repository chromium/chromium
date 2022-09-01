// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/privacy_hub_handler.h"

#include "base/bind.h"

namespace {

base::Value CameraPrivacySwitchStateToBaseValue(
    cros::mojom::CameraPrivacySwitchState state) {
  switch (state) {
    case cros::mojom::CameraPrivacySwitchState::ON:
      return base::Value(true);
    case cros::mojom::CameraPrivacySwitchState::OFF:
      return base::Value(false);
    case cros::mojom::CameraPrivacySwitchState::UNKNOWN:
      return base::Value();
  }
}

}  // namespace

namespace chromeos::settings {

PrivacyHubHandler::PrivacyHubHandler()
    : camera_privacy_switch_state_(media::CameraHalDispatcherImpl::GetInstance()
                                       ->AddCameraPrivacySwitchObserver(this)) {
  ui::MicrophoneMuteSwitchMonitor::Get()->AddObserver(this);
  CrasAudioHandler::Get()->AddAudioObserver(this);
}

PrivacyHubHandler::~PrivacyHubHandler() {
  media::CameraHalDispatcherImpl::GetInstance()
      ->RemoveCameraPrivacySwitchObserver(this);
  ui::MicrophoneMuteSwitchMonitor::Get()->RemoveObserver(this);
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

void PrivacyHubHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getInitialCameraHardwareToggleState",
      base::BindRepeating(&PrivacyHubHandler::HandleInitialCameraSwitchState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getInitialMicrophoneHardwareToggleState",
      base::BindRepeating(
          &PrivacyHubHandler::HandleInitialMicrophoneSwitchState,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getInitialAvailabilityOfMicrophoneForSimpleUsage",
      base::BindRepeating(
          &PrivacyHubHandler::
              HandleInitialAvailabilityOfMicrophoneForSimpleUsage,
          base::Unretained(this)));
}

void PrivacyHubHandler::OnAudioNodesChanged() {
  if (IsJavascriptAllowed()) {
    FireWebUIListener(
        "availability-of-microphone-for-simple-usage-changed",
        base::Value(
            CrasAudioHandler::Get()->HasActiveInputDeviceForSimpleUsage()));
  } else {
    DVLOG(1) << "JS disabled. Skip updating the availability of microphone for "
                "simple usage until enabled";
  }
}

void PrivacyHubHandler::OnCameraHWPrivacySwitchStatusChanged(
    int32_t camera_id,
    cros::mojom::CameraPrivacySwitchState state) {
  camera_privacy_switch_state_ = state;
  if (IsJavascriptAllowed()) {
    const base::Value value =
        CameraPrivacySwitchStateToBaseValue(camera_privacy_switch_state_);
    FireWebUIListener("camera-hardware-toggle-changed", value);
  } else {
    DVLOG(1) << "JS disabled. Skip camera privacy switch update until enabled";
  }
}

void PrivacyHubHandler::OnMicrophoneMuteSwitchValueChanged(bool muted) {
  if (IsJavascriptAllowed()) {
    FireWebUIListener("microphone-hardware-toggle-changed", base::Value(muted));
  } else {
    DVLOG(1) << "JS disabled. Skip microphone hardware privacy switch update "
                "until enabled";
  }
}

void PrivacyHubHandler::HandleInitialCameraSwitchState(
    const base::Value::List& args) {
  AllowJavascript();

  DCHECK_GE(1U, args.size()) << ": Did not expect arguments";
  DCHECK_EQ(1U, args.size()) << ": Callback ID is required";
  const auto& callback_id = args[0];
  const base::Value value =
      CameraPrivacySwitchStateToBaseValue(camera_privacy_switch_state_);

  ResolveJavascriptCallback(callback_id, value);
}

void PrivacyHubHandler::HandleInitialMicrophoneSwitchState(
    const base::Value::List& args) {
  AllowJavascript();

  DCHECK_GE(1U, args.size()) << ": Did not expect arguments";
  DCHECK_EQ(1U, args.size()) << ": Callback ID is required";
  const auto& callback_id = args[0];
  const base::Value value = base::Value(
      ui::MicrophoneMuteSwitchMonitor::Get()->microphone_mute_switch_on());

  ResolveJavascriptCallback(callback_id, value);
}

void PrivacyHubHandler::HandleInitialAvailabilityOfMicrophoneForSimpleUsage(
    const base::Value::List& args) {
  AllowJavascript();

  DCHECK_GE(1U, args.size()) << ": Did not expect arguments";
  DCHECK_EQ(1U, args.size()) << ": Callback ID is required";
  const auto& callback_id = args[0];
  const base::Value value = base::Value(
      CrasAudioHandler::Get()->HasActiveInputDeviceForSimpleUsage());

  ResolveJavascriptCallback(callback_id, value);
}

}  // namespace chromeos::settings
