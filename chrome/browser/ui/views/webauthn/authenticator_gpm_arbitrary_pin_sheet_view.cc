// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_gpm_arbitrary_pin_sheet_view.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/ui/views/webauthn/authenticator_common_views.h"
#include "chrome/browser/ui/views/webauthn/authenticator_gpm_arbitrary_pin_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"

AuthenticatorGPMArbitraryPinSheetView::AuthenticatorGPMArbitraryPinSheetView(
    std::unique_ptr<AuthenticatorGpmArbitraryPinSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {}

AuthenticatorGPMArbitraryPinSheetView::
    ~AuthenticatorGPMArbitraryPinSheetView() = default;

AuthenticatorGpmArbitraryPinSheetModel*
AuthenticatorGPMArbitraryPinSheetView::gpm_arbitrary_pin_sheet_model() {
  return static_cast<AuthenticatorGpmArbitraryPinSheetModel*>(model());
}

std::unique_ptr<views::View>
AuthenticatorGPMArbitraryPinSheetView::BuildStepSpecificHeader() {
  return CreateGpmIconWithLabel();
}

std::pair<std::unique_ptr<views::View>,
          AuthenticatorGPMArbitraryPinSheetView::AutoFocus>
AuthenticatorGPMArbitraryPinSheetView::BuildStepSpecificContent() {
  bool ui_disabled = gpm_arbitrary_pin_sheet_model()->ui_disabled();
  return std::make_pair(
      std::make_unique<AuthenticatorGPMArbitraryPinView>(
          ui_disabled, gpm_arbitrary_pin_sheet_model()->pin(),
          gpm_arbitrary_pin_sheet_model()->GetAccessibleName(),
          gpm_arbitrary_pin_sheet_model()->GetAccessibleDescription(), this),
      ui_disabled ? AutoFocus::kNo : AutoFocus::kYes);
}

int AuthenticatorGPMArbitraryPinSheetView::
    GetSpacingBetweenTitleAndDescription() {
  return kWebAuthnGpmDialogSpacingBetweenTitleAndDescription;
}

void AuthenticatorGPMArbitraryPinSheetView::OnPinChanged(std::u16string pin) {
  gpm_arbitrary_pin_sheet_model()->SetPin(std::move(pin));
}

BEGIN_METADATA(AuthenticatorGPMArbitraryPinSheetView)
END_METADATA
