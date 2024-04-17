// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_CREATE_GPM_PASSKEY_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_CREATE_GPM_PASSKEY_SHEET_VIEW_H_

#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"

// Represents a sheet in the Web Authentication request dialog that allows the
// user to create a passkey in Google Password Manager.
class AuthenticatorCreateGpmPasskeySheetView
    : public AuthenticatorRequestSheetView {
 public:
  explicit AuthenticatorCreateGpmPasskeySheetView(
      std::unique_ptr<AuthenticatorCreateGpmPasskeySheetModel> sheet_model);

  AuthenticatorCreateGpmPasskeySheetView(
      const AuthenticatorCreateGpmPasskeySheetView&) = delete;
  AuthenticatorCreateGpmPasskeySheetView& operator=(
      const AuthenticatorCreateGpmPasskeySheetView&) = delete;

  ~AuthenticatorCreateGpmPasskeySheetView() override;

 private:
  // AuthenticatorRequestSheetView:
  std::pair<std::unique_ptr<views::View>, AutoFocus> BuildStepSpecificContent()
      override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_CREATE_GPM_PASSKEY_SHEET_VIEW_H_
