// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_SELECT_ACCOUNT_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_SELECT_ACCOUNT_SHEET_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/account_hover_list_model.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"

// Web Authentication request dialog sheet view for selecting between one or
// more accounts.
class AuthenticatorSelectAccountSheetView
    : public AuthenticatorRequestSheetView,
      public AccountHoverListModel::Delegate {
 public:
  explicit AuthenticatorSelectAccountSheetView(
      std::unique_ptr<AuthenticatorSelectAccountSheetModel> model);
  ~AuthenticatorSelectAccountSheetView() override;

 private:
  AuthenticatorSelectAccountSheetModel* model() {
    return static_cast<AuthenticatorSelectAccountSheetModel*>(
        AuthenticatorRequestSheetView::model());
  }

  // AuthenticatorRequestSheetView:
  std::unique_ptr<views::View> BuildStepSpecificContent() override;

  // AccountHoverListModel::Delegate:
  void OnItemSelected(int index) override;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorSelectAccountSheetView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_SELECT_ACCOUNT_SHEET_VIEW_H_
