// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VERIFY_PENDING_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VERIFY_PENDING_DIALOG_VIEW_H_

namespace content {
class WebContents;
}

namespace autofill {

class VerifyPendingDialogController;

// The dialog to show card verification is in progress.
class VerifyPendingDialogView {
 public:
  // Factory function implemented by dialog's view implementation.
  static VerifyPendingDialogView* CreateDialogAndShow(
      VerifyPendingDialogController* controller,
      content::WebContents* web_contents);

  // Close the dialog and prevent callbacks being invoked.
  virtual void Hide() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VERIFY_PENDING_DIALOG_VIEW_H_
