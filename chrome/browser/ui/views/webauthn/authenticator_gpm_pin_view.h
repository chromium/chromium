// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_PIN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_PIN_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/webauthn/pin_textfield.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class ToggleImageButton;
}  // namespace views

// View showing a textfield for entering a numeric only GPM pin.
class AuthenticatorGPMPinView : public views::View,
                                public views::TextfieldController {
  METADATA_HEADER(AuthenticatorGPMPinView, views::View)

 public:
  class Delegate {
   public:
    virtual void OnPinChanged(std::u16string pin) = 0;
  };

  explicit AuthenticatorGPMPinView(Delegate* delegate, int pin_digits_count);

  AuthenticatorGPMPinView(const AuthenticatorGPMPinView&) = delete;
  AuthenticatorGPMPinView& operator=(const AuthenticatorGPMPinView&) = delete;
  ~AuthenticatorGPMPinView() override;

 private:
  // views::View:
  void RequestFocus() override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  void OnRevealButtonClicked();

  const raw_ptr<Delegate> delegate_;
  // Child view displaying textfield for the pin entry.
  raw_ptr<PinTextfield> pin_textfield_ = nullptr;
  // Child view displaying toggle for revealing the pin.
  raw_ptr<views::ToggleImageButton> reveal_button_ = nullptr;
  bool pin_revealed_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_PIN_VIEW_H_
