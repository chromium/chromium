// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_AUTOFILL_ERROR_DIALOG_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_AUTOFILL_ERROR_DIALOG_CONTROLLER_H_

#include <string>

#include "base/memory/weak_ptr.h"

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

  virtual base::WeakPtr<AutofillErrorDialogController> GetWeakPtr() = 0;

 protected:
  virtual ~AutofillErrorDialogController() = default;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_AUTOFILL_ERROR_DIALOG_CONTROLLER_H_
