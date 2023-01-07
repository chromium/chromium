// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/privacy_hub_handler.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"

namespace ash::settings {

namespace {
// Translates CameraPrivacySwitch state into base::Value
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

PrivacyHubHandler::PrivacyHubHandler() = default;

PrivacyHubHandler::~PrivacyHubHandler() {
  privacy_hub_util::SetFrontend(nullptr);
}

void PrivacyHubHandler::RegisterMessages() {
  privacy_hub_util::SetFrontend(this);
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

void PrivacyHubHandler::NotifyJS(const std::string& event_name,
                                 const base::Value& value) {
  if (IsJavascriptAllowed()) {
    FireWebUIListener(event_name, value);
  } else {
    DVLOG(1) << "JS disabled. Skip \"" << event_name
             << "\" event until enabled.";
  }
}

void PrivacyHubHandler::HandleInitialCameraSwitchState(
    const base::Value::List& args) {
  AllowJavascript();

  DCHECK_GE(1U, args.size()) << ": Did not expect arguments";
  DCHECK_EQ(1U, args.size()) << ": Callback ID is required";
  const auto& callback_id = args[0];
  const base::Value value = CameraPrivacySwitchStateToBaseValue(
      privacy_hub_util::CameraHWSwitchState());

  ResolveJavascriptCallback(callback_id, value);
}

void PrivacyHubHandler::HandleInitialMicrophoneSwitchState(
    const base::Value::List& args) {
  AllowJavascript();

  DCHECK_GE(1U, args.size()) << ": Did not expect arguments";
  DCHECK_EQ(1U, args.size()) << ": Callback ID is required";
  const auto& callback_id = args[0];
  const base::Value value =
      base::Value(privacy_hub_util::MicrophoneSwitchState());

  ResolveJavascriptCallback(callback_id, value);
}

void PrivacyHubHandler::HandleInitialAvailabilityOfMicrophoneForSimpleUsage(
    const base::Value::List& args) {
  AllowJavascript();

  DCHECK_GE(1U, args.size()) << ": Did not expect arguments";
  DCHECK_EQ(1U, args.size()) << ": Callback ID is required";
  const auto& callback_id = args[0];

  const base::Value value =
      base::Value(privacy_hub_util::HasActiveInputDeviceForSimpleUsage());

  ResolveJavascriptCallback(callback_id, value);
}

void PrivacyHubHandler::AvailabilityOfMicrophoneChanged(
    bool has_active_input_device) {
  NotifyJS("availability-of-microphone-for-simple-usage-changed",
           base::Value(has_active_input_device));
}

void PrivacyHubHandler::MicrophoneHardwareToggleChanged(bool muted) {
  NotifyJS("microphone-hardware-toggle-changed", base::Value(muted));
}

void PrivacyHubHandler::CameraHardwareToggleChanged(
    cros::mojom::CameraPrivacySwitchState state) {
  NotifyJS("camera-hardware-toggle-changed",
           CameraPrivacySwitchStateToBaseValue(state));
}

}  // namespace ash::settings
