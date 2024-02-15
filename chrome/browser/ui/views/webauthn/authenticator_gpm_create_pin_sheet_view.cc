// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_gpm_create_pin_sheet_view.h"

#include "base/strings/string_util.h"
#include "chrome/browser/ui/views/webauthn/pin_textfield.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/textfield/textfield.h"

namespace {
// Number of digits for the GPM Pin.
constexpr int kPinDigitCount = 6;
}  // namespace

AuthenticatorGpmCreatePinSheetView::AuthenticatorGpmCreatePinSheetView(
    std::unique_ptr<AuthenticatorGPMCreatePinSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {}

AuthenticatorGpmCreatePinSheetView::~AuthenticatorGpmCreatePinSheetView() =
    default;

bool AuthenticatorGpmCreatePinSheetView::HandleKeyEvent(
    views::Textfield* textfield,
    const ui::KeyEvent& event) {
  if (event.type() != ui::ET_KEY_PRESSED) {
    return false;
  }

  if (event.key_code() == ui::VKEY_BACK) {
    pin_textfield_->RemoveDigit();
    return true;
  }

  char16_t c = event.GetCharacter();
  if (base::IsAsciiDigit(c)) {
    pin_textfield_->AppendDigit(std::u16string(1, c));
  }

  return true;
}

std::pair<std::unique_ptr<views::View>,
          AuthenticatorGpmCreatePinSheetView::AutoFocus>
AuthenticatorGpmCreatePinSheetView::BuildStepSpecificContent() {
  auto pin_textfield = std::make_unique<PinTextfield>(kPinDigitCount);
  pin_textfield->SetController(this);
  pin_textfield->SetAccessibleName(u"Pin field (UNTRANSLATED)");
  pin_textfield_ = pin_textfield.get();
  return std::make_pair(std::move(pin_textfield), AutoFocus::kYes);
}
