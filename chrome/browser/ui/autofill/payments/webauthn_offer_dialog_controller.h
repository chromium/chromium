// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_CONTROLLER_H_

#include "base/macros.h"

namespace content {
class WebContents;
}

namespace autofill {

// An interface that exposes necessary controller functionality to
// WebauthnOfferDialogView.
class WebauthnOfferDialogController {
 public:
  WebauthnOfferDialogController() = default;
  virtual ~WebauthnOfferDialogController() = default;

  virtual void OnOkButtonClicked() = 0;

  virtual void OnCancelButtonClicked() = 0;

  virtual void OnDialogClosed() = 0;

  virtual content::WebContents* GetWebContents() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebauthnOfferDialogController);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_CONTROLLER_H_
