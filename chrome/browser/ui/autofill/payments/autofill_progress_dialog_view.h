// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_VIEW_H_

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

class AutofillProgressDialogController;

// The cross-platform view interface which helps show a progress bar (spinner)
// for autofill flows.
class AutofillProgressDialogView {
 public:
  AutofillProgressDialogView();
  virtual ~AutofillProgressDialogView();

  // Called by the controller to dismiss the dialog. If
  // `show_confirmation_before_closing` is true, we will show a confirmation
  // screen before dismissing the progress dialog. This confirms that we were
  // able to successfully authenticate the user using risk-based authentication,
  // which has no interactive authentication.
  virtual void Dismiss(bool show_confirmation_before_closing,
                       bool is_canceled_by_user) = 0;

  // Factory function for creating and showing the view.
  static AutofillProgressDialogView* CreateAndShow(
      AutofillProgressDialogController* controller);

  virtual void InvalidateControllerForCallbacks() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_VIEW_H_
