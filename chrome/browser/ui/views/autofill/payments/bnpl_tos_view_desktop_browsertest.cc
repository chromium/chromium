// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/bnpl_tos_view_desktop.h"

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

namespace autofill {

class BnplTosViewDesktopBrowserTest : public DialogBrowserTest {
 public:
  BnplTosViewDesktopBrowserTest() = default;
  ~BnplTosViewDesktopBrowserTest() override = default;
  BnplTosViewDesktopBrowserTest(const BnplTosViewDesktopBrowserTest&) = delete;
  BnplTosViewDesktopBrowserTest& operator=(
      const BnplTosViewDesktopBrowserTest&) = delete;

  void ShowUi(const std::string& name) override { client()->ShowBnplTos(); }

  payments::ChromePaymentsAutofillClient* client() const {
    ChromeAutofillClient* client =
        ChromeAutofillClient::FromWebContentsForTesting(web_contents());
    return static_cast<payments::ChromePaymentsAutofillClient*>(
        client->GetPaymentsAutofillClient());
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

// Ensures that when the BNPL ToS UI is shown, it won't crash the browser.
IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopBrowserTest, ShowAndVerifyUi) {
  ShowAndVerifyUi();
}

// Ensures that closing the current tab while the dialog is visible won't crash
// the browser.
IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopBrowserTest, CloseTab) {
  ShowAndVerifyUi();
  // Close the tab.
  web_contents()->Close();
  // Wait until tab is closed.
  base::RunLoop().RunUntilIdle();
}

// Ensures that closing the window while the dialog is visible won't crash the
// browser.
IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopBrowserTest, CloseWindow) {
  ShowAndVerifyUi();
  // Close the browser window.
  browser()->window()->Close();
  // Wait until the browser window is closed.
  base::RunLoop().RunUntilIdle();
}

}  // namespace autofill
