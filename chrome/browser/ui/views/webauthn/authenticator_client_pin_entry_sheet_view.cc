// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_client_pin_entry_sheet_view.h"

#include <memory>
#include <utility>

#include "base/logging.h"

AuthenticatorClientPinEntrySheetView::AuthenticatorClientPinEntrySheetView(
    std::unique_ptr<AuthenticatorClientPinEntrySheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {
  pin_entry_sheet_model()->SetDelegate(this);
}

AuthenticatorClientPinEntrySheetView::~AuthenticatorClientPinEntrySheetView() =
    default;

AuthenticatorClientPinEntrySheetModel*
AuthenticatorClientPinEntrySheetView::pin_entry_sheet_model() {
  return static_cast<AuthenticatorClientPinEntrySheetModel*>(model());
}

std::unique_ptr<views::View>
AuthenticatorClientPinEntrySheetView::BuildStepSpecificContent() {
  DCHECK(!pin_entry_view_);
  auto view = std::make_unique<AuthenticatorClientPinEntryView>(
      this, pin_entry_sheet_model()->mode() ==
                AuthenticatorClientPinEntrySheetModel::Mode::
                    kPinSetup /* show_confirmation_text_field */);
  pin_entry_view_ = view.get();
  pin_entry_sheet_model()->MaybeShowRetryError();
  return view;
}

void AuthenticatorClientPinEntrySheetView::OnPincodeChanged(
    base::string16 pincode) {
  pin_entry_sheet_model()->SetPinCode(std::move(pincode));
}

void AuthenticatorClientPinEntrySheetView::OnConfirmationChanged(
    base::string16 pincode) {
  pin_entry_sheet_model()->SetPinConfirmation(std::move(pincode));
}

void AuthenticatorClientPinEntrySheetView::ShowPinError(
    const base::string16& error) {
  if (!pin_entry_view_) {
    NOTREACHED();
    return;
  }
  pin_entry_view_->UpdateError(error);
}
