// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_H_

#include <memory>

namespace autofill {

class AutofillDialogController;

// The view of the Autofill dialog that is responsible for showing the UI and
// passing events to the controller.
class AutofillDialogView {
 public:
  // Factory function to create the view.
  static std::unique_ptr<AutofillDialogView> Create(
      AutofillDialogController* controller);

  virtual ~AutofillDialogView() = default;

  // Show the dialog.
  virtual void Show() = 0;

  // Dismiss the dialog.
  virtual void Dismiss() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_H_
