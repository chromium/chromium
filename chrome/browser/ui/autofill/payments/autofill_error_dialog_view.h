// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_ERROR_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_ERROR_DIALOG_VIEW_H_

namespace autofill {

class AutofillErrorDialogController;

// The cross-platform view interface which helps show an error dialog for
// autofill flows.
//
// Note: This is only used for virtual card related errors.
class AutofillErrorDialogView {
 public:
  virtual ~AutofillErrorDialogView() = default;

  virtual void Dismiss() = 0;

  // Factory function for creating and showing the view.
  static AutofillErrorDialogView* CreateAndShow(
      AutofillErrorDialogController* controller);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_ERROR_DIALOG_VIEW_H_
