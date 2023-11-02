// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_VIEW_H_

namespace autofill {

class AutofillProgressDialogController;

// The cross-platform view interface which helps show a progress bar (spinner)
// for autofill flows.
class AutofillProgressDialogView {
 public:
  virtual ~AutofillProgressDialogView() = default;

  // Called by the controller to dismiss the dialog.
  virtual void Dismiss(bool show_confirmation_before_closing,
                       bool is_canceled_by_user) = 0;

  // Factory function for creating and showing the view.
  static AutofillProgressDialogView* CreateAndShow(
      AutofillProgressDialogController* controller);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_VIEW_H_
