// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"

#include <string>

std::vector<std::u16string>
AuthenticatorRequestSheetModel::GetAdditionalDescriptions() const {
  return {};
}

std::u16string AuthenticatorRequestSheetModel::GetError() const {
  return std::u16string();
}

std::u16string AuthenticatorRequestSheetModel::GetHint() const {
  return std::u16string();
}

bool AuthenticatorRequestSheetModel::IsManageDevicesButtonVisible() const {
  return false;
}

bool AuthenticatorRequestSheetModel::IsOtherMechanismButtonVisible() const {
  return false;
}

bool AuthenticatorRequestSheetModel::IsForgotGPMPinButtonVisible() const {
  return false;
}

bool AuthenticatorRequestSheetModel::IsGPMPinOptionsButtonVisible() const {
  return false;
}

std::u16string AuthenticatorRequestSheetModel::GetOtherMechanismButtonLabel()
    const {
  return std::u16string();
}

void AuthenticatorRequestSheetModel::OnManageDevices() {}

void AuthenticatorRequestSheetModel::OnForgotGPMPin() const {}

void AuthenticatorRequestSheetModel::OnGPMPinOptionChosen(
    bool is_arbitrary) const {}
