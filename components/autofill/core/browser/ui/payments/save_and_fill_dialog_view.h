// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_AND_FILL_DIALOG_VIEW_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_AND_FILL_DIALOG_VIEW_H_

namespace autofill {

// Interface that exposes the view to SaveAndFillControllerImpl.
class SaveAndFillDialogView {
 public:
  virtual ~SaveAndFillDialogView() = default;

  // A pending throbber is shown while the preflight call is executed. This
  // function is called once a response is received, signaling the view to
  // transition to the appropriate version of the dialog (local or upload)
  // based on the controller's updated state.
  virtual void DismissThrobberAndUpdateMainView() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_AND_FILL_DIALOG_VIEW_H_
