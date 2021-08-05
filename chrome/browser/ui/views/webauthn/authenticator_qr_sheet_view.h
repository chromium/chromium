// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_QR_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_QR_SHEET_VIEW_H_

#include <memory>

#include "base/containers/span.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"

class AuthenticatorQRViewCentered;

class AuthenticatorQRSheetView : public AuthenticatorRequestSheetView {
 public:
  explicit AuthenticatorQRSheetView(
      std::unique_ptr<AuthenticatorQRSheetModel> model);

  ~AuthenticatorQRSheetView() override;

  // RefreshQRCode causes a fresh QR code to be painted.
  void RefreshQRCode(base::span<const uint8_t> new_qr_data);

 private:
  // AuthenticatorRequestSheetView:
  std::pair<std::unique_ptr<views::View>, AutoFocus> BuildStepSpecificContent()
      override;
  void Update();

  AuthenticatorQRViewCentered* qr_view_ = nullptr;
  const std::string qr_string_;
  base::RepeatingTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorQRSheetView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_QR_SHEET_VIEW_H_
