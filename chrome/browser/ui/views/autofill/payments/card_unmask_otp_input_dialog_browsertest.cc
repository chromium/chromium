// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/card_unmask_otp_input_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/card_unmask_otp_input_dialog_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

namespace autofill {

namespace {
const int kDefaultOtpLength = 6;
}  // namespace

class CardUnmaskOtpInputDialogBrowserTest : public DialogBrowserTest {
 public:
  CardUnmaskOtpInputDialogBrowserTest() = default;
  CardUnmaskOtpInputDialogBrowserTest(
      const CardUnmaskOtpInputDialogBrowserTest&) = delete;
  CardUnmaskOtpInputDialogBrowserTest& operator=(
      const CardUnmaskOtpInputDialogBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Do lazy initialization of controller.
    CardUnmaskOtpInputDialogControllerImpl::CreateForWebContents(web_contents);
    controller()->ShowDialog(kDefaultOtpLength);
  }

  CardUnmaskOtpInputDialogView* GetDialog() {
    if (!controller())
      return nullptr;

    CardUnmaskOtpInputDialogView* dialog_view =
        controller()->GetDialogViewForTesting();
    if (!dialog_view)
      return nullptr;

    return static_cast<CardUnmaskOtpInputDialogView*>(dialog_view);
  }

  CardUnmaskOtpInputDialogControllerImpl* controller() {
    if (!browser() || !browser()->tab_strip_model() ||
        !browser()->tab_strip_model()->GetActiveWebContents()) {
      return nullptr;
    }
    return CardUnmaskOtpInputDialogControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }
};

IN_PROC_BROWSER_TEST_F(CardUnmaskOtpInputDialogBrowserTest,
                       InvokeUi_CardUnmaskOtpInputDialogDisplays) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CardUnmaskOtpInputDialogBrowserTest,
                       CanCloseTabWhileDialogShowing) {
  ShowUi("");
  VerifyUi();
  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(CardUnmaskOtpInputDialogBrowserTest,
                       CanCloseBrowserWhileDialogShowing) {
  ShowUi("");
  VerifyUi();
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();
}

}  // namespace autofill
