// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_VIEW_H_

namespace autofill {

class WebauthnOfferDialogController;
class WebauthnOfferDialogModel;

// The dialog to offer the option of using device's platform authenticator
// instead of CVC to verify the card in the future. Returns the reference to the
// model of the dialog.
class WebauthnOfferDialogView {
 public:
  static WebauthnOfferDialogView* CreateAndShow(
      WebauthnOfferDialogController* controller);

  virtual WebauthnOfferDialogModel* GetDialogModel() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_VIEW_H_
