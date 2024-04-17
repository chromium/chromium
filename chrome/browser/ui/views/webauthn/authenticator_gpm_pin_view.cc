// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_gpm_pin_view.h"

#include "base/strings/string_util.h"
#include "chrome/browser/ui/views/webauthn/reveal_button_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

AuthenticatorGPMPinView::AuthenticatorGPMPinView(int pin_digits_count,
                                                 bool ui_disabled,
                                                 const std::u16string& pin,
                                                 Delegate* delegate)
    : delegate_(delegate) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto pin_textfield = std::make_unique<PinTextfield>(pin_digits_count);
  pin_textfield->SetController(this);
  pin_textfield->SetAccessibleName(u"Pin field (UNTRANSLATED)");
  pin_textfield->SetObscured(true);
  pin_textfield->SetDisabled(ui_disabled);
  pin_textfield->SetPin(pin);
  pin_textfield->SetEnabled(!ui_disabled);
  pin_textfield_ = AddChildView(std::move(pin_textfield));

  reveal_button_ = AddChildView(CreateRevealButton(
      base::BindRepeating(&AuthenticatorGPMPinView::OnRevealButtonClicked,
                          base::Unretained(this))));
  reveal_button_->SetEnabled(!ui_disabled);
}

AuthenticatorGPMPinView::~AuthenticatorGPMPinView() = default;

void AuthenticatorGPMPinView::RequestFocus() {
  pin_textfield_->RequestFocus();
}

bool AuthenticatorGPMPinView::HandleKeyEvent(views::Textfield* textfield,
                                             const ui::KeyEvent& event) {
  if (event.type() != ui::ET_KEY_PRESSED) {
    return false;
  }

  bool pin_changed = false;
  char16_t c = event.GetCharacter();
  if (base::IsAsciiDigit(c)) {
    pin_changed = pin_textfield_->AppendDigit(std::u16string(1, c));
  } else if (event.key_code() == ui::VKEY_BACK) {
    pin_changed = pin_textfield_->RemoveDigit();
  }

  if (pin_changed) {
    delegate_->OnPinChanged(pin_textfield_->GetPin());
  }

  return true;
}

void AuthenticatorGPMPinView::OnRevealButtonClicked() {
  pin_revealed_ = !pin_revealed_;
  reveal_button_->SetToggled(pin_revealed_);
  pin_textfield_->SetObscured(!pin_revealed_);
}

BEGIN_METADATA(AuthenticatorGPMPinView)
END_METADATA
