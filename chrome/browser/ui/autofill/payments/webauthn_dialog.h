// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_H_

namespace autofill {

class WebauthnDialogController;
class WebauthnDialogModel;
enum class WebauthnDialogState;

// The dialog that offers the option to use device's platform
// authenticator. It is shown automatically after card unmasked details are
// obtained and filled into the form.
class WebauthnDialog {
 public:
  static WebauthnDialog* CreateAndShow(WebauthnDialogController* controller,
                                       WebauthnDialogState dialog_state);

  virtual WebauthnDialogModel* GetDialogModel() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_H_
