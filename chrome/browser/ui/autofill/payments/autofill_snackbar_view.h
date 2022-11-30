// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_SNACKBAR_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_SNACKBAR_VIEW_H_

namespace autofill {

// The UI interface which shows a snackbar after a form is autofilled on
// Android.
class AutofillSnackbarView {
 public:
  virtual void Show() = 0;
  virtual void Dismiss() = 0;

  // Factory function for creating the view.
  static AutofillSnackbarView* Create(AutofillSnackbarController* controller);

 protected:
  virtual ~AutofillSnackbarView() = default;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_SNACKBAR_VIEW_H_
