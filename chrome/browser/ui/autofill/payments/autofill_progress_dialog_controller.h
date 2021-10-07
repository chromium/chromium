// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_CONTROLLER_H_

#include <string>

namespace content {
class WebContents;
}

namespace autofill {

// Interface that exposes controller functionality to
// AutofillProgressDialogView. The interface exposes the title, description and
// the button label to the view to help show a progress dialog with a single
// button that acts as a cancel button. For example: We show a progress dialog
// when contacting the bank during unmasking a virtual card.
class AutofillProgressDialogController {
 public:
  // Callback received when the progress dialog is dismissed.
  virtual void OnDismissed() = 0;

  // Title and button label.
  virtual const std::u16string GetTitle() = 0;
  virtual const std::u16string GetCancelButtonLabel() = 0;

  // Text displayed below the progress bar.
  virtual const std::u16string GetLoadingMessage() = 0;
  virtual const std::u16string GetConfirmationMessage() = 0;

  virtual content::WebContents* GetWebContents() = 0;

 protected:
  virtual ~AutofillProgressDialogController() = default;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_CONTROLLER_H_
