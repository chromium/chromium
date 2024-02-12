// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_gpm_create_pin_sheet_view.h"

#include "ui/views/controls/textfield/textfield.h"

AuthenticatorGpmCreatePinSheetView::AuthenticatorGpmCreatePinSheetView(
    std::unique_ptr<AuthenticatorGPMCreatePinSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {}

AuthenticatorGpmCreatePinSheetView::~AuthenticatorGpmCreatePinSheetView() =
    default;

std::pair<std::unique_ptr<views::View>,
          AuthenticatorGpmCreatePinSheetView::AutoFocus>
AuthenticatorGpmCreatePinSheetView::BuildStepSpecificContent() {
  // TODO(rgod): Add proper implementation once its designed.
  auto pin_textfield = std::make_unique<views::Textfield>();
  pin_textfield->SetAccessibleName(u"Pin field (UNTRANSLATED)");
  return std::make_pair(std::move(pin_textfield), AutoFocus::kYes);
}
