// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_PRIORITY_MECHANISM_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_PRIORITY_MECHANISM_SHEET_VIEW_H_

#include <memory>

#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "ui/base/metadata/metadata_header_macros.h"

// Web Authentication request dialog sheet view for confirming selection of a
// "priority" mechanism.
class AuthenticatorPriorityMechanismSheetView
    : public AuthenticatorRequestSheetView {
  METADATA_HEADER(AuthenticatorPriorityMechanismSheetView,
                  AuthenticatorRequestSheetView)

 public:
  explicit AuthenticatorPriorityMechanismSheetView(
      std::unique_ptr<AuthenticatorPriorityMechanismSheetModel> model);

  AuthenticatorPriorityMechanismSheetView(
      const AuthenticatorPriorityMechanismSheetView&) = delete;
  AuthenticatorPriorityMechanismSheetView& operator=(
      const AuthenticatorPriorityMechanismSheetView&) = delete;

  ~AuthenticatorPriorityMechanismSheetView() override;

 private:
  // AuthenticatorRequestSheetView:
  std::pair<std::unique_ptr<views::View>, AutoFocus> BuildStepSpecificContent()
      override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_PRIORITY_MECHANISM_SHEET_VIEW_H_
