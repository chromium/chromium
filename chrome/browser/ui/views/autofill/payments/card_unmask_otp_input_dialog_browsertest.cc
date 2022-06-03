// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/payments/card_unmask_otp_input_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/card_unmask_otp_input_dialog_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/card_unmask_otp_input_dialog_views.h"
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
    controller()->ShowDialog(kDefaultOtpLength, /*delegate=*/nullptr);
  }

  CardUnmaskOtpInputDialogViews* GetDialogViews() {
    if (!controller())
      return nullptr;

    CardUnmaskOtpInputDialogView* dialog_view =
        controller()->GetDialogViewForTesting();
    if (!dialog_view)
      return nullptr;

    return static_cast<CardUnmaskOtpInputDialogViews*>(dialog_view);
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

// Ensures the UI can be shown.
#if defined(OS_WIN)
// Triggering logic required for Windows OS runs: https://crbug.com/1254686
#define MAYBE_InvokeUi_CardUnmaskOtpInputDialogDisplays \
  DISABLED_InvokeUi_CardUnmaskOtpInputDialogDisplays
#else
#define MAYBE_InvokeUi_CardUnmaskOtpInputDialogDisplays \
  InvokeUi_CardUnmaskOtpInputDialogDisplays
#endif
IN_PROC_BROWSER_TEST_F(CardUnmaskOtpInputDialogBrowserTest,
                       MAYBE_InvokeUi_CardUnmaskOtpInputDialogDisplays) {
  base::HistogramTester histogram_tester;

  ShowAndVerifyUi();

  // TODO(crbug.com/1243475): Move this logging to controller unittest as well.
  // Right now the view is created but not injected. Need to change this when
  // moving this logging.
  histogram_tester.ExpectUniqueSample("Autofill.OtpInputDialog.SmsOtp.Shown",
                                      true, 1);
}

// Ensures closing tab while dialog being visible is correctly handled.
#if defined(OS_WIN)
// Triggering logic required for Windows OS runs: https://crbug.com/1254686
#define MAYBE_CanCloseTabWhileDialogShowing \
  DISABLED_CanCloseTabWhileDialogShowing
#else
#define MAYBE_CanCloseTabWhileDialogShowing CanCloseTabWhileDialogShowing
#endif
IN_PROC_BROWSER_TEST_F(CardUnmaskOtpInputDialogBrowserTest,
                       MAYBE_CanCloseTabWhileDialogShowing) {
  ShowUi("");
  VerifyUi();
  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures closing browser while dialog being visible is correctly handled.
#if defined(OS_WIN)
// Triggering logic required for Windows OS runs: https://crbug.com/1254686
#define MAYBE_CanCloseBrowserWhileDialogShowing \
  DISABLED_CanCloseBrowserWhileDialogShowing
#else
#define MAYBE_CanCloseBrowserWhileDialogShowing \
  CanCloseBrowserWhileDialogShowing
#endif
IN_PROC_BROWSER_TEST_F(CardUnmaskOtpInputDialogBrowserTest,
                       MAYBE_CanCloseBrowserWhileDialogShowing) {
  ShowUi("");
  VerifyUi();
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();
}

}  // namespace autofill
