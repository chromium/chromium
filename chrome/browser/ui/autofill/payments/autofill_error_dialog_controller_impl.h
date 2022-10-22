// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_ERROR_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_ERROR_DIALOG_CONTROLLER_IMPL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_controller.h"
#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_view.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
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

  // Show the error dialog for the given |autofill_error_dialog_context|.
  void Show(const AutofillErrorDialogContext& autofill_error_dialog_context);

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

  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;
  // The context of the error dialog that is being displayed. Contains
  // information such as the type of the error dialog that is being displayed.
  // |error_dialog_context_| may also contain extra information such as a
  // detailed title and description returned from the server. If
  // |error_dialog_context_| contains this information, the fields in
  // |error_dialog_context_| should be preferred when displaying the error
  // dialog.
  AutofillErrorDialogContext error_dialog_context_;
  // View that displays the error dialog.
  raw_ptr<AutofillErrorDialogView> autofill_error_dialog_view_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_ERROR_DIALOG_CONTROLLER_IMPL_H_
