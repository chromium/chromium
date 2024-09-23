// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_SNACKBAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_SNACKBAR_CONTROLLER_H_
#include <string>

#include "content/public/browser/web_contents.h"

namespace autofill {

// Controller interface that exposes snackbar functionality to autofill views.
class AutofillSnackbarController {
 public:
  virtual ~AutofillSnackbarController() = default;

  // User clicked the action shown on the snackbar.
  virtual void OnActionClicked() = 0;
  // The snackbar was dismissed without any user interaction.
  virtual void OnDismissed() = 0;

  // Returns the text to be displayed in the message area of the snackbar.
  virtual std::u16string GetMessageText() const = 0;
  // Returns the text to be displayed in the action button of the snackbar.
  virtual std::u16string GetActionButtonText() const = 0;
  // Return the duration for which the snackbar should be shown.
  virtual base::TimeDelta GetDuration() const = 0;
  virtual content::WebContents* GetWebContents() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_SNACKBAR_CONTROLLER_H_
