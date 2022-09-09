// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_CONTROLLER_H_

namespace content {
class WebContents;
}

namespace autofill {

// An interface that exposes necessary controller functionality to
// WebauthnDialogView.
class WebauthnDialogController {
 public:
  virtual ~WebauthnDialogController() = default;

  virtual void OnOkButtonClicked() = 0;

  virtual void OnCancelButtonClicked() = 0;

  virtual void OnDialogClosed() = 0;

  virtual content::WebContents* GetWebContents() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_CONTROLLER_H_
