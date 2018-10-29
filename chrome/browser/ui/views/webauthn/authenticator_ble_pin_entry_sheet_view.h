// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_BLE_PIN_ENTRY_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_BLE_PIN_ENTRY_SHEET_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/ble_pin_entry_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"

// Represents a sheet in the Web Authentication request dialog that allows the
// user to input pin code used to connect to BLE security key.
class AuthenticatorBlePinEntrySheetView : public AuthenticatorRequestSheetView,
                                          public BlePinEntryView::Delegate {
 public:
  explicit AuthenticatorBlePinEntrySheetView(
      std::unique_ptr<AuthenticatorBlePinEntrySheetModel> model);
  ~AuthenticatorBlePinEntrySheetView() override;

 private:
  // AuthenticatorRequestSheetView:
  std::unique_ptr<views::View> BuildStepSpecificContent() override;

  // BlePinEntryView::Delegate:
  void OnPincodeChanged(base::string16 pincode) override;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorBlePinEntrySheetView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_BLE_PIN_ENTRY_SHEET_VIEW_H_
