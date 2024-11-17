// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_MULTI_SOURCE_PICKER_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_MULTI_SOURCE_PICKER_SHEET_VIEW_H_

#include <memory>

#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "ui/base/metadata/metadata_header_macros.h"

// Web Authentication request dialog sheet view for selecting between multiple
// accounts and mechanisms.
class AuthenticatorMultiSourcePickerSheetView
    : public AuthenticatorRequestSheetView {
  METADATA_HEADER(AuthenticatorMultiSourcePickerSheetView,
                  AuthenticatorRequestSheetView)

 public:
  explicit AuthenticatorMultiSourcePickerSheetView(
      std::unique_ptr<AuthenticatorMultiSourcePickerSheetModel> model);

  AuthenticatorMultiSourcePickerSheetView(
      const AuthenticatorMultiSourcePickerSheetView&) = delete;
  AuthenticatorMultiSourcePickerSheetView& operator=(
      const AuthenticatorMultiSourcePickerSheetView&) = delete;

  ~AuthenticatorMultiSourcePickerSheetView() override;

 private:
  // AuthenticatorRequestSheetView:
  std::pair<std::unique_ptr<views::View>, AutoFocus> BuildStepSpecificContent()
      override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_MULTI_SOURCE_PICKER_SHEET_VIEW_H_
