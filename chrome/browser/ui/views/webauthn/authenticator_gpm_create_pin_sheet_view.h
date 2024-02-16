// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_CREATE_PIN_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_CREATE_PIN_SHEET_VIEW_H_

#include "chrome/browser/ui/views/webauthn/authenticator_gpm_pin_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"

// Represents a sheet in the Web Authentication request dialog that allows the
// user to create GPM pin code used as a recovery factor in passkeys flow.
class AuthenticatorGpmCreatePinSheetView
    : public AuthenticatorRequestSheetView,
      public AuthenticatorGPMPinView::Delegate {
 public:
  explicit AuthenticatorGpmCreatePinSheetView(
      std::unique_ptr<AuthenticatorGPMCreatePinSheetModel> sheet_model);

  AuthenticatorGpmCreatePinSheetView(
      const AuthenticatorGpmCreatePinSheetView&) = delete;
  AuthenticatorGpmCreatePinSheetView& operator=(
      const AuthenticatorGpmCreatePinSheetView&) = delete;

  ~AuthenticatorGpmCreatePinSheetView() override;

 private:
  AuthenticatorGPMCreatePinSheetModel* gpm_pin_sheet_model();

  // AuthenticatorRequestSheetView:
  std::pair<std::unique_ptr<views::View>, AutoFocus> BuildStepSpecificContent()
      override;

  // AuthenticatorGPMPinView::Delegate:
  void OnPinChanged(std::u16string pin) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_CREATE_PIN_SHEET_VIEW_H_
