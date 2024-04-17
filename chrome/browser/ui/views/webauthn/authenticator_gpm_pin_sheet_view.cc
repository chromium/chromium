// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_gpm_pin_sheet_view.h"

#include "chrome/browser/ui/views/webauthn/authenticator_gpm_pin_view.h"

AuthenticatorGpmPinSheetView::AuthenticatorGpmPinSheetView(
    std::unique_ptr<AuthenticatorGPMPinSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {}

AuthenticatorGpmPinSheetView::~AuthenticatorGpmPinSheetView() = default;

AuthenticatorGPMPinSheetModel*
AuthenticatorGpmPinSheetView::gpm_pin_sheet_model() {
  return static_cast<AuthenticatorGPMPinSheetModel*>(model());
}

std::pair<std::unique_ptr<views::View>, AuthenticatorGpmPinSheetView::AutoFocus>
AuthenticatorGpmPinSheetView::BuildStepSpecificContent() {
  bool ui_disabled = gpm_pin_sheet_model()->ui_disabled();
  return std::make_pair(std::make_unique<AuthenticatorGPMPinView>(
                            gpm_pin_sheet_model()->pin_digits_count(),
                            ui_disabled, gpm_pin_sheet_model()->pin(), this),
                        ui_disabled ? AutoFocus::kNo : AutoFocus::kYes);
}

void AuthenticatorGpmPinSheetView::OnPinChanged(std::u16string pin) {
  gpm_pin_sheet_model()->SetPin(std::move(pin));
}
