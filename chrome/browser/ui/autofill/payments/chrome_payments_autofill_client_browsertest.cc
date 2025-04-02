// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

enum DialogEnum { BnplTos, SelectBnplIssuer };

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
      case DialogEnum::BnplTos: {
        BnplTosModel model;
        model.issuer = test::GetTestUnlinkedBnplIssuer();
        client()->ShowBnplTos(std::move(model), base::DoNothing(),
                              base::DoNothing());
        break;
      }
      case DialogEnum::SelectBnplIssuer: {
        client()->ShowSelectBnplIssuerDialog(
            {payments::BnplIssuerContext(
                test::GetTestUnlinkedBnplIssuer(),
                payments::BnplIssuerEligibilityForPage::kIsEligible)},
            /*app_locale=*/"en-US", base::DoNothing(), base::DoNothing());
        break;
      }
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

INSTANTIATE_TEST_SUITE_P(
    ,
    ChromePaymentsAutofillClientBrowserTest,
    testing::Values(DialogTestData{"BNPL_ToS", DialogEnum::BnplTos},
                    DialogTestData{"Select_BNPL_Issuer",
                                   DialogEnum::SelectBnplIssuer}),
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
