// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/webauthn_offer_dialog_view_impl.h"

namespace autofill {

class WebauthnOfferDialogBrowserTest : public DialogBrowserTest {
 public:
  WebauthnOfferDialogBrowserTest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Do lazy initialization of WebauthnOfferDialogControllerImpl.
    WebauthnOfferDialogControllerImpl::CreateForWebContents(web_contents);
    DCHECK(controller());
    controller()->ShowOfferDialog(base::DoNothing());
  }

  WebauthnOfferDialogViewImpl* GetWebauthnOfferDialog() {
    if (!controller())
      return nullptr;

    WebauthnOfferDialogView* dialog_view = controller()->dialog_view();
    if (!dialog_view)
      return nullptr;

    return static_cast<WebauthnOfferDialogViewImpl*>(dialog_view);
  }

  WebauthnOfferDialogControllerImpl* controller() {
    if (!browser() || !browser()->tab_strip_model() ||
        !browser()->tab_strip_model()->GetActiveWebContents())
      return nullptr;

    return WebauthnOfferDialogControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebauthnOfferDialogBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebauthnOfferDialogBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

// Ensures closing tab while dialog being visible is correctly handled.
// RunUntilIdle() makes sure that nothing crashes after browser tab is closed.
IN_PROC_BROWSER_TEST_F(WebauthnOfferDialogBrowserTest,
                       CanCloseTabWhileDialogShowing) {
  ShowUi(std::string());
  VerifyUi();
  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures closing browser while dialog being visible is correctly handled.
IN_PROC_BROWSER_TEST_F(WebauthnOfferDialogBrowserTest,
                       CanCloseBrowserWhileDialogShowing) {
  ShowUi(std::string());
  VerifyUi();
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures dialog is closed when cancel button is clicked.
IN_PROC_BROWSER_TEST_F(WebauthnOfferDialogBrowserTest, ClickCancelButton) {
  ShowUi(std::string());
  VerifyUi();
  GetWebauthnOfferDialog()->CancelDialog();
  base::RunLoop().RunUntilIdle();
}

}  // namespace autofill
