// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/privacy_hub_handler.h"

#include "base/bind.h"

namespace {

base::Value privacySwitchStateToBaseValue(
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
}

void PrivacyHubHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getInitialCameraHardwareToggleState",
      base::BindRepeating(&PrivacyHubHandler::HandleInitial,
                          base::Unretained(this)));
}

void PrivacyHubHandler::OnCameraPrivacySwitchStatusChanged(
    cros::mojom::CameraPrivacySwitchState state) {
  camera_privacy_switch_state_ = state;
  if (IsJavascriptAllowed()) {
    const base::Value value =
        privacySwitchStateToBaseValue(camera_privacy_switch_state_);
    FireWebUIListener("camera-hardware-toggle-changed", value);
  } else {
    DVLOG(1) << "JS disabled. Skip camera privacy switch update until enabled";
  }
}

void PrivacyHubHandler::HandleInitial(const base::Value::List& args) {
  AllowJavascript();
  const auto& callback_id = args[0];
  const base::Value value =
      privacySwitchStateToBaseValue(camera_privacy_switch_state_);
  ResolveJavascriptCallback(callback_id, value);
}

}  // namespace chromeos::settings
