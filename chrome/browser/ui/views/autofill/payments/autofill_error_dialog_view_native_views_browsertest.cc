// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/autofill_error_dialog_view_native_views.h"
#include "content/public/test/browser_test.h"

namespace autofill {

class AutofillErrorDialogViewNativeViewsBrowserTest : public DialogBrowserTest {
 public:
  AutofillErrorDialogViewNativeViewsBrowserTest() = default;
  ~AutofillErrorDialogViewNativeViewsBrowserTest() override = default;
  AutofillErrorDialogViewNativeViewsBrowserTest(
      const AutofillErrorDialogViewNativeViewsBrowserTest&) = delete;
  AutofillErrorDialogViewNativeViewsBrowserTest& operator=(
      const AutofillErrorDialogViewNativeViewsBrowserTest&) = delete;

  // DialogBrowserTest:
  void SetUpOnMainThread() override {
    controller_ = std::make_unique<AutofillErrorDialogControllerImpl>(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  void ShowUi(const std::string& name) override {
    AutofillErrorDialogController::AutofillErrorDialogType dialog_type;
    if (name == "temporary") {
      dialog_type = AutofillErrorDialogController::AutofillErrorDialogType::
          VIRTUAL_CARD_TEMPORARY_ERROR;
    } else if (name == "permanent") {
      dialog_type = AutofillErrorDialogController::AutofillErrorDialogType::
          VIRTUAL_CARD_PERMANENT_ERROR;
    } else if (name == "eligibility") {
      dialog_type = AutofillErrorDialogController::AutofillErrorDialogType::
          VIRTUAL_CARD_NOT_ELIGIBLE_ERROR;
    } else {
      NOTREACHED();
      return;
    }

    controller()->Show(dialog_type);
  }

  AutofillErrorDialogViewNativeViews* GetDialogViews() {
    if (!controller())
      return nullptr;

    AutofillErrorDialogView* dialog_view =
        controller()->autofill_error_dialog_view();
    if (!dialog_view)
      return nullptr;

    return static_cast<AutofillErrorDialogViewNativeViews*>(dialog_view);
  }

  AutofillErrorDialogControllerImpl* controller() { return controller_.get(); }

 private:
  std::unique_ptr<AutofillErrorDialogControllerImpl> controller_;
};

IN_PROC_BROWSER_TEST_F(AutofillErrorDialogViewNativeViewsBrowserTest,
                       InvokeUi_temporary) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AutofillErrorDialogViewNativeViewsBrowserTest,
                       InvokeUi_permanent) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AutofillErrorDialogViewNativeViewsBrowserTest,
                       InvokeUi_eligibility) {
  ShowAndVerifyUi();
}

// Ensures closing current tab while dialog being visible is correctly handled.
IN_PROC_BROWSER_TEST_F(AutofillErrorDialogViewNativeViewsBrowserTest,
                       CloseTabWhileDialogShowing) {
  ShowUi("temporary");
  VerifyUi();
  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures closing browser while dialog being visible is correctly handled.
IN_PROC_BROWSER_TEST_F(AutofillErrorDialogViewNativeViewsBrowserTest,
                       CanCloseBrowserWhileDialogShowing) {
  ShowUi("temporary");
  VerifyUi();
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures clicking on the cancel button is correctly handled.
IN_PROC_BROWSER_TEST_F(AutofillErrorDialogViewNativeViewsBrowserTest,
                       ClickCancelButton) {
  ShowUi("temporary");
  VerifyUi();
  GetDialogViews()->CancelDialog();
  base::RunLoop().RunUntilIdle();
}

}  // namespace autofill
