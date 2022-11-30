// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_ERROR_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_ERROR_DIALOG_CONTROLLER_H_
#include <string>

#include "content/public/browser/web_contents.h"

namespace autofill {

// Interface that exposes controller functionality to AutofillErrorDialogView.
// The interface exposes the title, description and the button label to the view
// to help show an error dialog with a single button that acts as a cancel
// button. For example: We show an error dialog when unmasking a virtual card
// fails.
//
// Note: This is only used for virtual card related errors.
class AutofillErrorDialogController {
 public:
  // Callback received when the error dialog is dismissed.
  virtual void OnDismissed() = 0;

  // Title to displayed on the error dialog.
  virtual const std::u16string GetTitle() = 0;
  // Description of the error to be displayed below the title.
  virtual const std::u16string GetDescription() = 0;
  // Text for the positive button which cancels the dialog.
  virtual const std::u16string GetButtonLabel() = 0;
  virtual content::WebContents* GetWebContents() = 0;

 protected:
  virtual ~AutofillErrorDialogController() = default;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_ERROR_DIALOG_CONTROLLER_H_
