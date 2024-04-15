// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/payments/view_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/card_unmask_authentication_selection_dialog_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog_controller_impl.h"
#include "content/public/test/browser_test.h"

namespace autofill {

class CardUnmaskAuthenticationSelectionDialogBrowserTestBase
    : public DialogBrowserTest {
 public:
  CardUnmaskAuthenticationSelectionDialogBrowserTestBase() = default;
  CardUnmaskAuthenticationSelectionDialogBrowserTestBase(
      const CardUnmaskAuthenticationSelectionDialogBrowserTestBase&) = delete;
  CardUnmaskAuthenticationSelectionDialogBrowserTestBase& operator=(
      const CardUnmaskAuthenticationSelectionDialogBrowserTestBase&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    card_unmask_authentication_selection_dialog_controller_ =
        std::make_unique<CardUnmaskAuthenticationSelectionDialogControllerImpl>(
            challenge_options_,
            /*confirm_unmasking_method_callback=*/base::DoNothing(),
            /*cancel_unmasking_closure=*/base::DoNothing());
    controller()->ShowDialog(
        base::BindOnce(&CreateAndShowCardUnmaskAuthenticationSelectionDialog,
                       base::Unretained(web_contents)));
  }

  CardUnmaskAuthenticationSelectionDialogView* GetDialog() {
    if (!controller())
      return nullptr;

    CardUnmaskAuthenticationSelectionDialog* dialog_view =
        controller()->GetDialogViewForTesting();
    if (!dialog_view)
      return nullptr;

    return static_cast<CardUnmaskAuthenticationSelectionDialogView*>(
        dialog_view);
  }

  void SetChallengeOptions(
      std::vector<CardUnmaskChallengeOption> challenge_options) {
    challenge_options_ = std::move(challenge_options);
  }

  const std::vector<CardUnmaskChallengeOption>& GetChallengeOptions() {
    return challenge_options_;
  }

  CardUnmaskAuthenticationSelectionDialogControllerImpl* controller() {
    return card_unmask_authentication_selection_dialog_controller_.get();
  }

 protected:
  std::vector<CardUnmaskChallengeOption> challenge_options_;
  std::unique_ptr<CardUnmaskAuthenticationSelectionDialogControllerImpl>
      card_unmask_authentication_selection_dialog_controller_;
};

// Non-parameterized version of
// CardUnmaskAuthenticationSelectionDialogBrowserTestBase. Should be used to
// test the specific functionality of a certain type of challenge option being
// selected, instead of the overall functionality of the dialog.
// TODO(crbug.com/40247985): Add browser tests for specific SMS OTP challenge
// selection logging.
class CardUnmaskAuthenticationSelectionDialogBrowserTestNonParameterized
    : public CardUnmaskAuthenticationSelectionDialogBrowserTestBase {
 public:
  CardUnmaskAuthenticationSelectionDialogBrowserTestNonParameterized() =
      default;
  ~CardUnmaskAuthenticationSelectionDialogBrowserTestNonParameterized()
      override = default;
};

// Ensure accepting the CVC challenge option in the selection dialog is
// correctly handled.
IN_PROC_BROWSER_TEST_F(
    CardUnmaskAuthenticationSelectionDialogBrowserTestNonParameterized,
    AcceptedByUserAfterSelectingCvcAuthResultsMetricsLoggedAsExpected) {
  base::HistogramTester histogram_tester;
  SetChallengeOptions(test::GetCardUnmaskChallengeOptions(
      std::vector<CardUnmaskChallengeOptionType>{
          CardUnmaskChallengeOptionType::kSmsOtp,
          CardUnmaskChallengeOptionType::kCvc}));
  ShowUi("");
  VerifyUi();

  // Select the CVC challenge option in the dialog.
  auto cvc_challenge_option = base::ranges::find_if(
      GetChallengeOptions(), [](const auto& challenge_option) {
        return challenge_option.type == CardUnmaskChallengeOptionType::kCvc;
      });
  controller()->SetSelectedChallengeOptionId(cvc_challenge_option->id);

  // Accept the authentication selection dialog with the CVC challenge option
  // chosen.
  GetDialog()->Accept();
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Result",
      AutofillMetrics::CardUnmaskAuthenticationSelectionDialogResultMetric::
          kDismissedByUserAcceptanceNoServerRequestNeeded,
      1);
}

// Parameters of the
// CardUnmaskAuthenticationSelectionDialogBrowserTestParameterized.
using ChallengeOptionTypes = std::vector<CardUnmaskChallengeOptionType>;

// Parameterized version of
// CardUnmaskAuthenticationSelectionDialogBrowserTestBase. Should be used to
// test the overall functionality of the dialog, across all combinations of
// challenge options related to the dialog.
class CardUnmaskAuthenticationSelectionDialogBrowserTestParameterized
    : public CardUnmaskAuthenticationSelectionDialogBrowserTestBase,
      public testing::WithParamInterface<ChallengeOptionTypes> {
 public:
  CardUnmaskAuthenticationSelectionDialogBrowserTestParameterized() = default;
  ~CardUnmaskAuthenticationSelectionDialogBrowserTestParameterized() override =
      default;

  ChallengeOptionTypes GetChallengeOptionTypes() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    CardUnmaskAuthenticationSelectionDialogBrowserTestParameterized,
    testing::Values(
        std::vector<CardUnmaskChallengeOptionType>{
            CardUnmaskChallengeOptionType::kSmsOtp},
        std::vector<CardUnmaskChallengeOptionType>{
            CardUnmaskChallengeOptionType::kSmsOtp,
            CardUnmaskChallengeOptionType::kCvc,
            CardUnmaskChallengeOptionType::kThreeDomainSecure}));

// Ensures the UI can be shown.
IN_PROC_BROWSER_TEST_P(
    CardUnmaskAuthenticationSelectionDialogBrowserTestParameterized,
    InvokeUi_CardUnmaskAuthSelectionDialogDisplays) {
  base::HistogramTester histogram_tester;
  SetChallengeOptions(
      test::GetCardUnmaskChallengeOptions(GetChallengeOptionTypes()));
  ShowAndVerifyUi();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.CardUnmaskAuthenticationSelectionDialog.Shown2"),
      base::BucketsAre(base::Bucket(GetChallengeOptionTypes().size(), 1)));
}

// Ensures closing tab while dialog being visible is correctly handled.
IN_PROC_BROWSER_TEST_P(
    CardUnmaskAuthenticationSelectionDialogBrowserTestParameterized,
    CanCloseTabWhileDialogShowing) {
  base::HistogramTester histogram_tester;
  SetChallengeOptions(
      test::GetCardUnmaskChallengeOptions(GetChallengeOptionTypes()));
  ShowUi("");
  VerifyUi();
  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.CardUnmaskAuthenticationSelectionDialog.Shown2"),
      base::BucketsAre(base::Bucket(GetChallengeOptionTypes().size(), 1)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Result",
      AutofillMetrics::CardUnmaskAuthenticationSelectionDialogResultMetric::
          kCanceledByUserBeforeSelection,
      1);
}

// Ensures closing browser while dialog being visible is correctly handled.
IN_PROC_BROWSER_TEST_P(
    CardUnmaskAuthenticationSelectionDialogBrowserTestParameterized,
    CanCloseBrowserWhileDialogShowing) {
  base::HistogramTester histogram_tester;
  SetChallengeOptions(
      test::GetCardUnmaskChallengeOptions(GetChallengeOptionTypes()));
  ShowUi("");
  VerifyUi();
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.CardUnmaskAuthenticationSelectionDialog.Shown2"),
      base::BucketsAre(base::Bucket(GetChallengeOptionTypes().size(), 1)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Result",
      AutofillMetrics::CardUnmaskAuthenticationSelectionDialogResultMetric::
          kCanceledByUserBeforeSelection,
      1);
}

// Ensure cancelling dialog is correctly handled.
IN_PROC_BROWSER_TEST_P(
    CardUnmaskAuthenticationSelectionDialogBrowserTestParameterized,
    CanceledByUserAfterSelectionResultsMetricsLoggedAsExpected) {
  base::HistogramTester histogram_tester;
  SetChallengeOptions(
      test::GetCardUnmaskChallengeOptions(GetChallengeOptionTypes()));
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
