// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

enum DialogEnum { BnplTos };

struct DialogTestData {
  std::string name;
  DialogEnum dialog;
};

std::string GetTestName(const ::testing::TestParamInfo<DialogTestData>& info) {
  return info.param.name;
}

}  // namespace

class ChromePaymentsAutofillClientBrowserTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<DialogTestData> {
 public:
  ChromePaymentsAutofillClientBrowserTest() = default;
  ChromePaymentsAutofillClientBrowserTest(
      const ChromePaymentsAutofillClientBrowserTest&) = delete;
  ChromePaymentsAutofillClientBrowserTest& operator=(
      const ChromePaymentsAutofillClientBrowserTest&) = delete;
  ~ChromePaymentsAutofillClientBrowserTest() override = default;

  void ShowUi(const std::string& name) override {
    switch (GetParam().dialog) {
      case DialogEnum::BnplTos:
        client()->ShowBnplTos();
        break;
    }
  }

  payments::ChromePaymentsAutofillClient* client() const {
    return ChromeAutofillClient::FromWebContentsForTesting(web_contents())
        ->GetPaymentsAutofillClient();
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         ChromePaymentsAutofillClientBrowserTest,
                         testing::Values(DialogTestData{"BNPL_ToS",
                                                        DialogEnum::BnplTos}),
                         GetTestName);

// Ensures that the dialog is shown and it won't crash the browser.
IN_PROC_BROWSER_TEST_P(ChromePaymentsAutofillClientBrowserTest,
                       ShowAndVerifyUi) {
  ShowAndVerifyUi();
}

// Ensures that closing the current tab while the dialog is visible won't crash
// the browser.
IN_PROC_BROWSER_TEST_P(ChromePaymentsAutofillClientBrowserTest,
                       ShowAndVerifyUi_ThenCloseTab) {
  ShowAndVerifyUi();
  // Close the tab.
  web_contents()->Close();
  // Wait until tab is closed.
  base::RunLoop().RunUntilIdle();
}

// Ensures that closing the window while the dialog is visible won't crash the
// browser.
IN_PROC_BROWSER_TEST_P(ChromePaymentsAutofillClientBrowserTest,
                       ShowAndVerifyUi_ThenCloseWindow) {
  ShowAndVerifyUi();
  // Close the browser window.
  browser()->window()->Close();
  // Wait until the browser window is closed.
  base::RunLoop().RunUntilIdle();
}

}  // namespace autofill
