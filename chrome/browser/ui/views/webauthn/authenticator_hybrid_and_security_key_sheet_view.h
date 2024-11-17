// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_HYBRID_AND_SECURITY_KEY_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_HYBRID_AND_SECURITY_KEY_SHEET_VIEW_H_

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class AuthenticatorHybridAndSecurityKeySheetView
    : public AuthenticatorRequestSheetView {
  METADATA_HEADER(AuthenticatorHybridAndSecurityKeySheetView,
                  AuthenticatorRequestSheetView)

 public:
  explicit AuthenticatorHybridAndSecurityKeySheetView(
      std::unique_ptr<AuthenticatorHybridAndSecurityKeySheetModel> model);

  AuthenticatorHybridAndSecurityKeySheetView(
      const AuthenticatorHybridAndSecurityKeySheetView&) = delete;
  AuthenticatorHybridAndSecurityKeySheetView& operator=(
      const AuthenticatorHybridAndSecurityKeySheetView&) = delete;

  ~AuthenticatorHybridAndSecurityKeySheetView() override;

 private:
  // AuthenticatorRequestSheetView:
  std::pair<std::unique_ptr<views::View>, AutoFocus> BuildStepSpecificContent()
      override;

  const std::string qr_string_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_HYBRID_AND_SECURITY_KEY_SHEET_VIEW_H_
