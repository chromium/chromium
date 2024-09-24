// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_gpm_pin_sheet_view.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/ui/views/webauthn/authenticator_common_views.h"
#include "chrome/browser/ui/views/webauthn/authenticator_gpm_pin_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"

AuthenticatorGpmPinSheetView::AuthenticatorGpmPinSheetView(
    std::unique_ptr<AuthenticatorGpmPinSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {}

AuthenticatorGpmPinSheetView::~AuthenticatorGpmPinSheetView() = default;

AuthenticatorGpmPinSheetModel*
AuthenticatorGpmPinSheetView::gpm_pin_sheet_model() {
  return static_cast<AuthenticatorGpmPinSheetModel*>(model());
}

std::unique_ptr<views::View>
AuthenticatorGpmPinSheetView::BuildStepSpecificHeader() {
  return CreateGpmIconWithLabel();
}

std::pair<std::unique_ptr<views::View>, AuthenticatorGpmPinSheetView::AutoFocus>
AuthenticatorGpmPinSheetView::BuildStepSpecificContent() {
  bool ui_disabled = gpm_pin_sheet_model()->ui_disabled();
  return std::make_pair(
      std::make_unique<AuthenticatorGPMPinView>(
          gpm_pin_sheet_model()->pin_digits_count(), ui_disabled,
          gpm_pin_sheet_model()->pin(),
          gpm_pin_sheet_model()->GetAccessibleDescription(), this),
      ui_disabled ? AutoFocus::kNo : AutoFocus::kYes);
}

int AuthenticatorGpmPinSheetView::GetSpacingBetweenTitleAndDescription() {
  return kWebAuthnGpmDialogSpacingBetweenTitleAndDescription;
}

void AuthenticatorGpmPinSheetView::OnPinChanged(std::u16string pin) {
  gpm_pin_sheet_model()->SetPin(std::move(pin));
}

void AuthenticatorGpmPinSheetView::PinCharTyped(bool is_digit) {
  gpm_pin_sheet_model()->PinCharTyped(is_digit);
}

std::u16string AuthenticatorGpmPinSheetView::GetPinAccessibleName() {
  return gpm_pin_sheet_model()->GetAccessibleName();
}

BEGIN_METADATA(AuthenticatorGpmPinSheetView)
END_METADATA
