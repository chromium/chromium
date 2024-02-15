// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_CREATE_PIN_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_CREATE_PIN_SHEET_VIEW_H_

#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/pin_textfield.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "ui/views/controls/textfield/textfield_controller.h"

// Represents a sheet in the Web Authentication request dialog that allows the
// user to create GPM pin code used as a recovery factor in passkeys flow.
class AuthenticatorGpmCreatePinSheetView : public AuthenticatorRequestSheetView,
                                           public views::TextfieldController {
 public:
  explicit AuthenticatorGpmCreatePinSheetView(
      std::unique_ptr<AuthenticatorGPMCreatePinSheetModel> sheet_model);

  AuthenticatorGpmCreatePinSheetView(
      const AuthenticatorGpmCreatePinSheetView&) = delete;
  AuthenticatorGpmCreatePinSheetView& operator=(
      const AuthenticatorGpmCreatePinSheetView&) = delete;

  ~AuthenticatorGpmCreatePinSheetView() override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

 private:
  // AuthenticatorRequestSheetView:
  std::pair<std::unique_ptr<views::View>, AutoFocus> BuildStepSpecificContent()
      override;

  // Child view displaying textfield for the pin entry.
  raw_ptr<PinTextfield> pin_textfield_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_CREATE_PIN_SHEET_VIEW_H_
