// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_PAASK_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_PAASK_SHEET_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"

// AuthenticatorPaaskSheetView adds the description text to the dialog. This is
// done as step-specific content because the text includes a clickable link and
// thus needs special handling.
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
  std::unique_ptr<views::View> BuildStepSpecificContent() override;

  void OnLinkClicked();
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_PAASK_SHEET_VIEW_H_
