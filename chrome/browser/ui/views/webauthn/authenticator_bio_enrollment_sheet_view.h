// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_BIO_ENROLLMENT_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_BIO_ENROLLMENT_SHEET_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "ui/base/metadata/metadata_header_macros.h"

// Represents a sheet in the Web Authentication request dialog that allows the
// user to input pin code used to connect to BLE security key.
class AuthenticatorBioEnrollmentSheetView
    : public AuthenticatorRequestSheetView {
  METADATA_HEADER(AuthenticatorBioEnrollmentSheetView,
                  AuthenticatorRequestSheetView)

 public:
  explicit AuthenticatorBioEnrollmentSheetView(
      std::unique_ptr<AuthenticatorBioEnrollmentSheetModel> sheet_model);

  AuthenticatorBioEnrollmentSheetView(
      const AuthenticatorBioEnrollmentSheetView&) = delete;
  AuthenticatorBioEnrollmentSheetView& operator=(
      const AuthenticatorBioEnrollmentSheetView&) = delete;

  ~AuthenticatorBioEnrollmentSheetView() override;

 private:
  // AuthenticatorRequestSheetView:
  std::pair<std::unique_ptr<views::View>, AutoFocus> BuildStepSpecificContent()
      override;

  // views::View:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_BIO_ENROLLMENT_SHEET_VIEW_H_
