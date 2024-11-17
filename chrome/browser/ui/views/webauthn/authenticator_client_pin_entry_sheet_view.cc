// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_client_pin_entry_sheet_view.h"

#include <memory>
#include <utility>

#include "ui/base/metadata/metadata_impl_macros.h"

AuthenticatorClientPinEntrySheetView::AuthenticatorClientPinEntrySheetView(
    std::unique_ptr<AuthenticatorClientPinEntrySheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {
}

AuthenticatorClientPinEntrySheetView::~AuthenticatorClientPinEntrySheetView() =
    default;

AuthenticatorClientPinEntrySheetModel*
AuthenticatorClientPinEntrySheetView::pin_entry_sheet_model() {
  return static_cast<AuthenticatorClientPinEntrySheetModel*>(model());
}

std::pair<std::unique_ptr<views::View>,
          AuthenticatorRequestSheetView::AutoFocus>
AuthenticatorClientPinEntrySheetView::BuildStepSpecificContent() {
  return std::make_pair(
      std::make_unique<AuthenticatorClientPinEntryView>(
          this,
          /*show_confirmation_text_field=*/pin_entry_sheet_model()->mode() !=
              AuthenticatorClientPinEntrySheetModel::Mode::kPinEntry),
      AutoFocus::kYes);
}

void AuthenticatorClientPinEntrySheetView::OnPincodeChanged(
    std::u16string pincode) {
  pin_entry_sheet_model()->SetPinCode(std::move(pincode));
}

void AuthenticatorClientPinEntrySheetView::OnConfirmationChanged(
    std::u16string pincode) {
  pin_entry_sheet_model()->SetPinConfirmation(std::move(pincode));
}

BEGIN_METADATA(AuthenticatorClientPinEntrySheetView)
END_METADATA
