// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_CLIENT_PIN_ENTRY_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_CLIENT_PIN_ENTRY_SHEET_VIEW_H_

#include <memory>

#include "chrome/browser/ui/views/webauthn/authenticator_client_pin_entry_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "ui/base/metadata/metadata_header_macros.h"

// Web Authentication request dialog sheet view for entering an authenticator
// PIN.
class AuthenticatorClientPinEntrySheetView
    : public AuthenticatorRequestSheetView,
      public AuthenticatorClientPinEntryView::Delegate {
  METADATA_HEADER(AuthenticatorClientPinEntrySheetView,
                  AuthenticatorRequestSheetView)

 public:
  explicit AuthenticatorClientPinEntrySheetView(
      std::unique_ptr<AuthenticatorClientPinEntrySheetModel> model);

  AuthenticatorClientPinEntrySheetView(
      const AuthenticatorClientPinEntrySheetView&) = delete;
  AuthenticatorClientPinEntrySheetView& operator=(
      const AuthenticatorClientPinEntrySheetView&) = delete;

  ~AuthenticatorClientPinEntrySheetView() override;

 private:
  AuthenticatorClientPinEntrySheetModel* pin_entry_sheet_model();

  // AuthenticatorRequestSheetView:
  std::pair<std::unique_ptr<views::View>, AutoFocus> BuildStepSpecificContent()
      override;

  // AuthenticatorClientPinEntryView::Delegate:
  void OnPincodeChanged(std::u16string pincode) override;
  void OnConfirmationChanged(std::u16string pincode) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_CLIENT_PIN_ENTRY_SHEET_VIEW_H_
