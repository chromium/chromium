// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"

bool AuthenticatorRequestSheetModel::ShouldFocusBackArrow() const {
  return false;
}

bool AuthenticatorRequestSheetModel::IsCloseButtonVisible() const {
  return false;
}

std::u16string AuthenticatorRequestSheetModel::GetAdditionalDescription()
    const {
  return std::u16string();
}

std::u16string AuthenticatorRequestSheetModel::GetError() const {
  return std::u16string();
}

bool AuthenticatorRequestSheetModel::IsManageDevicesButtonVisible() const {
  return false;
}

bool AuthenticatorRequestSheetModel::IsOtherMechanismButtonVisible() const {
  return false;
}

void AuthenticatorRequestSheetModel::OnManageDevices() {}
