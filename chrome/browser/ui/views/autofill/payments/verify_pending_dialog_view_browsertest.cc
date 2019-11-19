// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/verify_pending_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/verify_pending_dialog_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/verify_pending_dialog_view_impl.h"

namespace autofill {

class VerifyPendingDialogViewBrowserTest : public DialogBrowserTest {
 public:
  VerifyPendingDialogViewBrowserTest() = default;
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    // Do lazy initialization of VerifyPendingDialogControllerImpl.
    VerifyPendingDialogControllerImpl::CreateForWebContents(web_contents);
    controller_ =
        VerifyPendingDialogControllerImpl::FromWebContents(web_contents);
    DCHECK(controller_);
    controller_->ShowDialog(base::DoNothing());
  }

  VerifyPendingDialogViewImpl* GetVerifyPendingDialog() {
    if (!controller_)
      return nullptr;
    VerifyPendingDialogView* verify_pending_dialog_view =
        controller_->dialog_view();
    if (!verify_pending_dialog_view)
      return nullptr;
    return static_cast<VerifyPendingDialogViewImpl*>(
        verify_pending_dialog_view);
  }

  VerifyPendingDialogControllerImpl* controller() { return controller_; }

 private:
  VerifyPendingDialogControllerImpl* controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(VerifyPendingDialogViewBrowserTest);
};

IN_PROC_BROWSER_TEST_F(VerifyPendingDialogViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

// This test ensures an edge case (closing tab while dialog being visible) is
// correctly handled, otherwise this test will crash during web contents being
// closed.
IN_PROC_BROWSER_TEST_F(VerifyPendingDialogViewBrowserTest,
                       CanCloseTabWhileDialogShowing) {
  ShowUi(std::string());
  VerifyUi();
  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures closing browser while dialog being visible is correctly handled.
IN_PROC_BROWSER_TEST_F(VerifyPendingDialogViewBrowserTest,
                       CanCloseBrowserWhileDialogShowing) {
  ShowUi(std::string());
  VerifyUi();
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures dialog can be closed when verification finishes.
IN_PROC_BROWSER_TEST_F(VerifyPendingDialogViewBrowserTest,
                       VerificationFinishes) {
  ShowUi(std::string());
  VerifyUi();
  GetVerifyPendingDialog()->Hide();
  base::RunLoop().RunUntilIdle();
}

// Ensures dialog is closed when cancel button is clicked.
IN_PROC_BROWSER_TEST_F(VerifyPendingDialogViewBrowserTest, ClickCancelButton) {
  ShowUi(std::string());
  VerifyUi();
  GetVerifyPendingDialog()->CancelDialog();
  base::RunLoop().RunUntilIdle();
}

// TODO(crbug.com/991037): Add more browser tests:
// 1. Make sure callback runs if cancel button clicked.

}  // namespace autofill
