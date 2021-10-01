// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_ERROR_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_ERROR_DIALOG_CONTROLLER_IMPL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_controller.h"
#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_view.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// Implementation of the AutofillErrorDialogController. This class allows error
// dialog to be shown or dismissed.
// The controller is destroyed once the view is dismissed.
class AutofillErrorDialogControllerImpl : public AutofillErrorDialogController {
 public:
  explicit AutofillErrorDialogControllerImpl(
      content::WebContents* web_contents);
  ~AutofillErrorDialogControllerImpl() override;

  AutofillErrorDialogControllerImpl(const AutofillErrorDialogControllerImpl&) =
      delete;
  AutofillErrorDialogControllerImpl& operator=(
      const AutofillErrorDialogControllerImpl&) = delete;

  // Show the error dialog for the given `AutofillErrorDialogType`
  void Show(
      AutofillErrorDialogController::AutofillErrorDialogType error_dialog_type);

  // AutofillErrorDialogController.
  void OnDismissed() override;
  const std::u16string GetTitle() override;
  const std::u16string GetDescription() override;
  const std::u16string GetButtonLabel() override;
  content::WebContents* GetWebContents() override;

  AutofillErrorDialogView* autofill_error_dialog_view() {
    return autofill_error_dialog_view_;
  }

 private:
  // Dismiss the error dialog if showing.
  void Dismiss();

  raw_ptr<content::WebContents> web_contents_;
  // The type of the error dialog that is being displayed.
  AutofillErrorDialogController::AutofillErrorDialogType error_dialog_type_;
  // View that displays the error dialog.
  raw_ptr<AutofillErrorDialogView> autofill_error_dialog_view_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_ERROR_DIALOG_CONTROLLER_IMPL_H_
