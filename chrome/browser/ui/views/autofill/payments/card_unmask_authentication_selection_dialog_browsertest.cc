// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/payments/card_unmask_authentication_selection_dialog_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/card_unmask_authentication_selection_dialog_views.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "content/public/test/browser_test.h"

namespace autofill {

class CardUnmaskAuthenticationSelectionDialogBrowserTest
    : public DialogBrowserTest {
 public:
  CardUnmaskAuthenticationSelectionDialogBrowserTest() = default;
  CardUnmaskAuthenticationSelectionDialogBrowserTest(
      const CardUnmaskAuthenticationSelectionDialogBrowserTest&) = delete;
  CardUnmaskAuthenticationSelectionDialogBrowserTest& operator=(
      const CardUnmaskAuthenticationSelectionDialogBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Do lazy initialization of controller.
    CardUnmaskAuthenticationSelectionDialogControllerImpl::CreateForWebContents(
        web_contents);
    controller()->ShowDialog(
        challenge_options_list_,
        /*confirm_unmasking_method_callback=*/base::DoNothing(),
        /*cancel_unmasking_closure=*/base::DoNothing());
  }

  CardUnmaskAuthenticationSelectionDialogViews* GetDialog() {
    if (!controller())
      return nullptr;

    CardUnmaskAuthenticationSelectionDialogView* dialog_view =
        controller()->GetDialogViewForTesting();
    if (!dialog_view)
      return nullptr;

    return static_cast<CardUnmaskAuthenticationSelectionDialogViews*>(
        dialog_view);
  }

  CardUnmaskAuthenticationSelectionDialogControllerImpl* controller() {
    if (!browser() || !browser()->tab_strip_model() ||
        !browser()->tab_strip_model()->GetActiveWebContents()) {
      return nullptr;
    }
    return CardUnmaskAuthenticationSelectionDialogControllerImpl::
        FromWebContents(browser()->tab_strip_model()->GetActiveWebContents());
  }

  void InitChallengeOptions() {
    CardUnmaskChallengeOption card_unmask_challenge_option;
    card_unmask_challenge_option.type = CardUnmaskChallengeOptionType::kSmsOtp;
    card_unmask_challenge_option.challenge_info = u"xxx-xxx-3547";
    challenge_options_list_ = {card_unmask_challenge_option};
  }

 private:
  std::vector<CardUnmaskChallengeOption> challenge_options_list_;
};

// Ensures the UI can be shown.
IN_PROC_BROWSER_TEST_F(CardUnmaskAuthenticationSelectionDialogBrowserTest,
                       InvokeUi_CardUnmaskAuthSelectionDialogDisplays) {
  base::HistogramTester histogram_tester;
  InitChallengeOptions();
  ShowAndVerifyUi();
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Shown", true, 1);
}

// Ensures closing tab while dialog being visible is correctly handled.
IN_PROC_BROWSER_TEST_F(CardUnmaskAuthenticationSelectionDialogBrowserTest,
                       CanCloseTabWhileDialogShowing) {
  base::HistogramTester histogram_tester;
  InitChallengeOptions();
  ShowUi("");
  VerifyUi();
  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Shown", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Result",
      AutofillMetrics::CardUnmaskAuthenticationSelectionDialogResultMetric::
          kCanceledByUserBeforeSelection,
      1);
}

// Ensures closing browser while dialog being visible is correctly handled.
IN_PROC_BROWSER_TEST_F(CardUnmaskAuthenticationSelectionDialogBrowserTest,
                       CanCloseBrowserWhileDialogShowing) {
  base::HistogramTester histogram_tester;
  InitChallengeOptions();
  ShowUi("");
  VerifyUi();
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Shown", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Result",
      AutofillMetrics::CardUnmaskAuthenticationSelectionDialogResultMetric::
          kCanceledByUserBeforeSelection,
      1);
}

IN_PROC_BROWSER_TEST_F(
    CardUnmaskAuthenticationSelectionDialogBrowserTest,
    CanceledByUserAfterSelectionResultsMetricsLoggedAsExpected) {
  base::HistogramTester histogram_tester;
  InitChallengeOptions();
  ShowUi("");
  VerifyUi();
  // Put the dialog in pending state.
  GetDialog()->Accept();
  // Close the browser while in pending state.
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Result",
      AutofillMetrics::CardUnmaskAuthenticationSelectionDialogResultMetric::
          kCanceledByUserAfterSelection,
      1);
}

}  // namespace autofill
