// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_AUTOFILL_ERROR_DIALOG_VIEW_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_AUTOFILL_ERROR_DIALOG_VIEW_H_

namespace content {
class WebContents;
}  // namespace content

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
};

// Factory function for creating and showing the view.
// Note: On Desktop the view's ownership is transferred to the widget, which
// deletes it on dismissal, so no lifecycle management is needed. However, on
// Android this is not the case, the view's implementation must delete itself
// when dismissed.
AutofillErrorDialogView* CreateAndShowAutofillErrorDialog(
    AutofillErrorDialogController* controller,
    content::WebContents* web_contents);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_AUTOFILL_ERROR_DIALOG_VIEW_H_
