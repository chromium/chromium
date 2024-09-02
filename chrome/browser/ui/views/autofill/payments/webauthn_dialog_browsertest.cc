// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/webauthn_dialog_view.h"
#include "content/public/test/browser_test.h"

namespace autofill {
namespace {

// If ShowAndVerifyUi() is used, the tests must be named as "InvokeUi_"
// appending these names.
constexpr char kOfferDialogName[] = "Offer";
constexpr char kVerifyDialogName[] = "Verify";

class WebauthnDialogBrowserTest : public DialogBrowserTest {
 public:
  WebauthnDialogBrowserTest() = default;

  WebauthnDialogBrowserTest(const WebauthnDialogBrowserTest&) = delete;
  WebauthnDialogBrowserTest& operator=(const WebauthnDialogBrowserTest&) =
      delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Do lazy initialization of WebauthnDialogControllerImpl.
    WebauthnDialogControllerImpl::CreateForPage(
        web_contents()->GetPrimaryPage());

    if (name == kOfferDialogName) {
      controller()->ShowOfferDialog(base::DoNothing());
    } else if (name == kVerifyDialogName) {
      controller()->ShowVerifyPendingDialog(base::DoNothing());
    }
  }

  WebauthnDialogView* GetWebauthnDialogView() {
    if (!controller())
      return nullptr;

    WebauthnDialog* dialog = controller()->dialog();
    if (!dialog) {
      return nullptr;
    }

    return static_cast<WebauthnDialogView*>(dialog);
  }

  WebauthnDialogControllerImpl* controller() {
    if (!browser() || !browser()->tab_strip_model() ||
        !browser()->tab_strip_model()->GetActiveWebContents())
      return nullptr;

    return WebauthnDialogControllerImpl::GetForPage(
        web_contents()->GetPrimaryPage());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

IN_PROC_BROWSER_TEST_F(WebauthnDialogBrowserTest, InvokeUi_Offer) {
  ShowAndVerifyUi();
}

// Ensures closing tab while dialog being visible is correctly handled.
// RunUntilIdle() makes sure that nothing crashes after browser tab is closed.
IN_PROC_BROWSER_TEST_F(WebauthnDialogBrowserTest,
                       OfferDialog_CanCloseTabWhileDialogShowing) {
  ShowUi(kOfferDialogName);
  VerifyUi();
  web_contents()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures closing browser while dialog being visible is correctly handled.
IN_PROC_BROWSER_TEST_F(WebauthnDialogBrowserTest,
                       OfferDialog_CanCloseBrowserWhileDialogShowing) {
  ShowUi(kOfferDialogName);
  VerifyUi();
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures dialog is closed when cancel button is clicked.
IN_PROC_BROWSER_TEST_F(WebauthnDialogBrowserTest,
                       OfferDialog_ClickCancelButton) {
  ShowUi(kOfferDialogName);
  VerifyUi();
  GetWebauthnDialogView()->CancelDialog();
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(WebauthnDialogBrowserTest, InvokeUi_Verify) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebauthnDialogBrowserTest,
                       VerifyPendingDialog_CanCloseTabWhileDialogShowing) {
  ShowUi(kVerifyDialogName);
  VerifyUi();
  web_contents()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures closing browser while dialog being visible is correctly handled.
IN_PROC_BROWSER_TEST_F(WebauthnDialogBrowserTest,
                       VerifyPendingDialog_CanCloseBrowserWhileDialogShowing) {
  ShowUi(kVerifyDialogName);
  VerifyUi();
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures dialog can be closed when verification finishes.
IN_PROC_BROWSER_TEST_F(WebauthnDialogBrowserTest,
                       VerifyPendingDialog_VerificationFinishes) {
  ShowUi(kVerifyDialogName);
  VerifyUi();
  controller()->CloseDialog();
  base::RunLoop().RunUntilIdle();
}

// Ensures dialog is closed when cancel button is clicked.
IN_PROC_BROWSER_TEST_F(WebauthnDialogBrowserTest,
                       VerifyPendingDialog_ClickCancelButton) {
  ShowUi(kVerifyDialogName);
  VerifyUi();
  GetWebauthnDialogView()->CancelDialog();
  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace autofill
