// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_TOUCH_ID_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_TOUCH_ID_VIEW_H_

#include <os/availability.h>

#include <optional>

#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "crypto/scoped_lacontext.h"
#include "ui/base/metadata/metadata_header_macros.h"

// Displays a sheet prompting the user to tap their Touch ID sensor to complete
// a passkey flow.
class API_AVAILABLE(macos(12.0)) AuthenticatorTouchIdView
    : public AuthenticatorRequestSheetView {
  METADATA_HEADER(AuthenticatorTouchIdView, AuthenticatorRequestSheetView)

 public:
  explicit AuthenticatorTouchIdView(
      std::unique_ptr<AuthenticatorTouchIdSheetModel> sheet_model);

  AuthenticatorTouchIdView(const AuthenticatorTouchIdView&) = delete;
  AuthenticatorTouchIdView& operator=(const AuthenticatorTouchIdView&) = delete;

  ~AuthenticatorTouchIdView() override;

 private:
  // Called after the user taps their Touch ID sensor.
  void OnTouchIDComplete(std::optional<crypto::ScopedLAContext> lacontext);

  // AuthenticatorRequestSheetView:
  std::unique_ptr<views::View> BuildStepSpecificHeader() override;
  std::pair<std::unique_ptr<views::View>, AutoFocus> BuildStepSpecificContent()
      override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_TOUCH_ID_VIEW_H_
