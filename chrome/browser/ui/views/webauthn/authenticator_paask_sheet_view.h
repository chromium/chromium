// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_PAASK_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_PAASK_SHEET_VIEW_H_

#include <memory>

#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"

// AuthenticatorPaaskSheetView adds a clickable link to the PaaSK dialog if
// AOA transport is enabled.
class AuthenticatorPaaskSheetView : public AuthenticatorRequestSheetView {
 public:
  explicit AuthenticatorPaaskSheetView(
      std::unique_ptr<AuthenticatorPaaskSheetModel> model);
  ~AuthenticatorPaaskSheetView() override;
  AuthenticatorPaaskSheetView(const AuthenticatorPaaskSheetView&) = delete;
  AuthenticatorPaaskSheetView& operator=(const AuthenticatorPaaskSheetView&) =
      delete;

 private:
  // AuthenticatorRequestSheetView:
  std::pair<std::unique_ptr<views::View>, AutoFocus> BuildStepSpecificContent()
      override;

  void OnLinkClicked(const ui::Event&);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_PAASK_SHEET_VIEW_H_
