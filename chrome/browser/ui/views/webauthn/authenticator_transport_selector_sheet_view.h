// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_TRANSPORT_SELECTOR_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_TRANSPORT_SELECTOR_SHEET_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/hover_list_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "chrome/browser/ui/webauthn/transport_hover_list_model.h"
#include "chrome/browser/webauthn/authenticator_transport.h"

// Represents a sheet in the Web Authentication request dialog that allows the
// user to pick the transport protocol over which they wish to use their
// security key.
class AuthenticatorTransportSelectorSheetView
    : public AuthenticatorRequestSheetView {
 public:
  explicit AuthenticatorTransportSelectorSheetView(
      std::unique_ptr<AuthenticatorTransportSelectorSheetModel> model);
  ~AuthenticatorTransportSelectorSheetView() override;

 private:
  AuthenticatorTransportSelectorSheetModel* model() {
    return static_cast<AuthenticatorTransportSelectorSheetModel*>(
        AuthenticatorRequestSheetView::model());
  }

  // AuthenticatorRequestSheetView:
  std::unique_ptr<views::View> BuildStepSpecificContent() override;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorTransportSelectorSheetView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_TRANSPORT_SELECTOR_SHEET_VIEW_H_
