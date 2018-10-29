// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_ble_pin_entry_sheet_view.h"

#include <utility>

AuthenticatorBlePinEntrySheetView::AuthenticatorBlePinEntrySheetView(
    std::unique_ptr<AuthenticatorBlePinEntrySheetModel> model)
    : AuthenticatorRequestSheetView(std::move(model)) {}

AuthenticatorBlePinEntrySheetView::~AuthenticatorBlePinEntrySheetView() =
    default;

std::unique_ptr<views::View>
AuthenticatorBlePinEntrySheetView::BuildStepSpecificContent() {
  return std::make_unique<BlePinEntryView>(this);
}

void AuthenticatorBlePinEntrySheetView::OnPincodeChanged(
    base::string16 pincode) {
  static_cast<AuthenticatorBlePinEntrySheetModel*>(model())->SetPinCode(
      std::move(pincode));
}
