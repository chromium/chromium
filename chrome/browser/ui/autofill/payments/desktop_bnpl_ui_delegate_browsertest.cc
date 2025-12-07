// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/desktop_bnpl_ui_delegate.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

enum class DialogEnum { kSelectBnplIssuer, kBnplTos, kSelectBnplIssuerUpdate };

struct DialogTestData {
  std::string name;
  DialogEnum dialog = DialogEnum::kSelectBnplIssuer;
};

std::string GetTestName(const ::testing::TestParamInfo<DialogTestData>& info) {
  return info.param.name;
}

}  // namespace

namespace autofill::payments {

class DesktopBnplUiDelegateBrowserTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<DialogTestData> {
 public:
  DesktopBnplUiDelegateBrowserTest() {
    if (GetParam().dialog == DialogEnum::kSelectBnplIssuerUpdate) {
      feature_list_.InitAndEnableFeature(
          features::kAutofillEnableAiBasedAmountExtraction);
    }
  }
  DesktopBnplUiDelegateBrowserTest(const DesktopBnplUiDelegateBrowserTest&) =
      delete;
  DesktopBnplUiDelegateBrowserTest& operator=(
      const DesktopBnplUiDelegateBrowserTest&) = delete;
  ~DesktopBnplUiDelegateBrowserTest() override = default;

  void ShowUi(const std::string& name) override {
    switch (GetParam().dialog) {
      case DialogEnum::kSelectBnplIssuer: {
        GetDesktopBnplUiDelegate()->ShowSelectBnplIssuerUi(
            {BnplIssuerContext(test::GetTestUnlinkedBnplIssuer(),
                               BnplIssuerEligibilityForPage::kIsEligible)},
            /*app_locale=*/"en-US", base::DoNothing(), base::DoNothing(),
            /*has_seen_ai_terms=*/false);
        break;
      }
      case DialogEnum::kBnplTos: {
        BnplTosModel model;
        model.issuer = test::GetTestUnlinkedBnplIssuer();
        GetDesktopBnplUiDelegate()->ShowBnplTosUi(
            std::move(model), base::DoNothing(), base::DoNothing());
        break;
      }
      case DialogEnum::kSelectBnplIssuerUpdate: {
        GetDesktopBnplUiDelegate()->ShowSelectBnplIssuerUi(
            {BnplIssuerContext(test::GetTestUnlinkedBnplIssuer(),
                               BnplIssuerEligibilityForPage::kIsEligible)},
            /*app_locale=*/"en-US", base::DoNothing(), base::DoNothing(),
            /*has_seen_ai_terms=*/true);
        GetDesktopBnplUiDelegate()->UpdateBnplIssuerDialogUi(
            {BnplIssuerContext(test::GetTestUnlinkedBnplIssuer(),
                               BnplIssuerEligibilityForPage::kIsEligible)});
        break;
      }
    }
  }

  void RemoveUi() {
    switch (GetParam().dialog) {
      case DialogEnum::kSelectBnplIssuer: {
        GetDesktopBnplUiDelegate()->RemoveSelectBnplIssuerOrProgressUi();
        break;
      }
      case DialogEnum::kBnplTos: {
        GetDesktopBnplUiDelegate()->RemoveBnplTosOrProgressUi();
        break;
      }
      case DialogEnum::kSelectBnplIssuerUpdate: {
        GetDesktopBnplUiDelegate()->RemoveSelectBnplIssuerOrProgressUi();
        break;
      }
    }
  }

  DesktopBnplUiDelegate* GetDesktopBnplUiDelegate() {
    return static_cast<DesktopBnplUiDelegate*>(
        ChromeAutofillClient::FromWebContents(web_contents())
            ->GetPaymentsAutofillClient()
            ->GetBnplUiDelegate());
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    DesktopBnplUiDelegateBrowserTest,
    testing::Values(DialogTestData{"Select_BNPL_Issuer",
                                   DialogEnum::kSelectBnplIssuer},
                    DialogTestData{"BNPL_ToS", DialogEnum::kBnplTos},
                    DialogTestData{"Select_BNPL_Issuer_Update",
                                   DialogEnum::kSelectBnplIssuerUpdate}),
    GetTestName);

// Ensures that the dialog is shown and it won't crash the browser.
IN_PROC_BROWSER_TEST_P(DesktopBnplUiDelegateBrowserTest, ShowAndVerifyUi) {
  ShowAndVerifyUi();
}

// Ensures that closing the current tab while the dialog is visible won't crash
// the browser.
IN_PROC_BROWSER_TEST_P(DesktopBnplUiDelegateBrowserTest,
                       ShowAndVerifyUi_ThenCloseTab) {
  ShowAndVerifyUi();
  // Close the tab.
  web_contents()->Close();
  // Wait until tab is closed.
  base::RunLoop().RunUntilIdle();
}

// Ensures that closing the window while the dialog is visible won't crash the
// browser.
IN_PROC_BROWSER_TEST_P(DesktopBnplUiDelegateBrowserTest,
                       ShowAndVerifyUi_ThenCloseWindow) {
  ShowAndVerifyUi();
  // Close the browser window.
  browser()->window()->Close();
  // Wait until the browser window is closed.
  base::RunLoop().RunUntilIdle();
}

// Ensures that removing the dialog after showing won't crash the browser.
IN_PROC_BROWSER_TEST_P(DesktopBnplUiDelegateBrowserTest,
                       ShowAndVerifyUi_ThenCloseUi) {
  ShowAndVerifyUi();
  // Remove the dialog.
  RemoveUi();
}

}  // namespace autofill::payments
