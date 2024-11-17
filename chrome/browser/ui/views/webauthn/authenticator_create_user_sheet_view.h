// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_CREATE_USER_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_CREATE_USER_SHEET_VIEW_H_

#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "ui/base/metadata/metadata_header_macros.h"

// Represents a sheet in the Web Authentication request dialog that displays the
// username.
class AuthenticatorCreateUserSheetView : public AuthenticatorRequestSheetView {
  METADATA_HEADER(AuthenticatorCreateUserSheetView,
                  AuthenticatorRequestSheetView)

 public:
  explicit AuthenticatorCreateUserSheetView(
      std::unique_ptr<AuthenticatorSheetModelBase> sheet_model);

  AuthenticatorCreateUserSheetView(const AuthenticatorCreateUserSheetView&) =
      delete;
  AuthenticatorCreateUserSheetView& operator=(
      const AuthenticatorCreateUserSheetView&) = delete;

  ~AuthenticatorCreateUserSheetView() override;

 private:
  // AuthenticatorRequestSheetView:
  std::pair<std::unique_ptr<views::View>, AutoFocus> BuildStepSpecificContent()
      override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_CREATE_USER_SHEET_VIEW_H_
