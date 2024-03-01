// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_gpm_arbitrary_pin_sheet_view.h"

#include "chrome/browser/ui/views/webauthn/authenticator_client_pin_entry_view.h"

AuthenticatorGPMArbitraryPinSheetView::AuthenticatorGPMArbitraryPinSheetView(
    std::unique_ptr<AuthenticatorGPMArbitraryPinSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {}

AuthenticatorGPMArbitraryPinSheetView::
    ~AuthenticatorGPMArbitraryPinSheetView() = default;

AuthenticatorGPMArbitraryPinSheetModel*
AuthenticatorGPMArbitraryPinSheetView::gpm_arbitrary_pin_sheet_model() {
  return static_cast<AuthenticatorGPMArbitraryPinSheetModel*>(model());
}

std::pair<std::unique_ptr<views::View>,
          AuthenticatorGPMArbitraryPinSheetView::AutoFocus>
AuthenticatorGPMArbitraryPinSheetView::BuildStepSpecificContent() {
  // TODO(rgod): Use different view when mocks are ready or re-use and possibly
  // rename client pin.
  return std::make_pair(
      std::make_unique<AuthenticatorClientPinEntryView>(
          this, /*show_confirmation_text_field=*/gpm_arbitrary_pin_sheet_model()
                        ->mode() ==
                    AuthenticatorGPMArbitraryPinSheetModel::Mode::kPinCreate),
      AutoFocus::kYes);
}

void AuthenticatorGPMArbitraryPinSheetView::OnPincodeChanged(
    std::u16string pin_code) {
  gpm_arbitrary_pin_sheet_model()->SetPin(std::move(pin_code));
}

void AuthenticatorGPMArbitraryPinSheetView::OnConfirmationChanged(
    std::u16string pin_confirmation) {
  gpm_arbitrary_pin_sheet_model()->SetPinConfirmation(
      std::move(pin_confirmation));
}
