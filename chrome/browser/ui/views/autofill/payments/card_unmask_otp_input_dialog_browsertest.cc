// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/payments/payments_view_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/card_unmask_otp_input_dialog_views.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller_impl.h"
#include "content/public/test/browser_test.h"

namespace autofill {
namespace {

const int kDefaultOtpLength = 6;

class CardUnmaskOtpInputDialogBrowserTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<
          std::tuple<CardUnmaskChallengeOptionType, CreditCard::RecordType>> {
 public:
  CardUnmaskOtpInputDialogBrowserTest() = default;
  CardUnmaskOtpInputDialogBrowserTest(
      const CardUnmaskOtpInputDialogBrowserTest&) = delete;
  CardUnmaskOtpInputDialogBrowserTest& operator=(
      const CardUnmaskOtpInputDialogBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    CardUnmaskChallengeOption challenge_option;
    challenge_option.challenge_input_length = kDefaultOtpLength;
    challenge_option.type = std::get<0>(GetParam());
    CreditCard::RecordType card_type = std::get<1>(GetParam());
    controller_ = std::make_unique<CardUnmaskOtpInputDialogControllerImpl>(
        card_type, challenge_option, /*delegate=*/nullptr);
    controller_->ShowDialog(base::BindOnce(
        &CreateAndShowOtpInputDialog, controller_->GetWeakPtr(),
        base::Unretained(
            browser()->tab_strip_model()->GetActiveWebContents())));
  }

  CardUnmaskOtpInputDialogViews* GetDialog() {
    if (!controller_) {
      return nullptr;
    }

    base::WeakPtr<CardUnmaskOtpInputDialogView> dialog_view =
        controller_->GetDialogViewForTesting();
    if (!dialog_view) {
      return nullptr;
    }

    return static_cast<CardUnmaskOtpInputDialogViews*>(dialog_view.get());
  }

  std::string GetOtpAuthType() {
    switch (std::get<0>(GetParam())) {
      case CardUnmaskChallengeOptionType::kSmsOtp:
        return "SmsOtp";
      case CardUnmaskChallengeOptionType::kEmailOtp:
        return "EmailOtp";
      default:
        NOTREACHED();
    }
  }

  std::string GetCardType() {
    switch (std::get<1>(GetParam())) {
      case CreditCard::RecordType::kVirtualCard:
        return "VirtualCard";
      case CreditCard::RecordType::kFullServerCard:
      case CreditCard::RecordType::kMaskedServerCard:
        return "ServerCard";
      case CreditCard::RecordType::kLocalCard:
        return "LocalCard";
      default:
        NOTREACHED();
    }
  }

 private:
  std::unique_ptr<CardUnmaskOtpInputDialogControllerImpl> controller_;
};

// Ensures the UI can be shown.
IN_PROC_BROWSER_TEST_P(CardUnmaskOtpInputDialogBrowserTest,
                       InvokeUi_CardUnmaskOtpInputDialogDisplays) {
  base::HistogramTester histogram_tester;

  ShowAndVerifyUi();

  // TODO(crbug.com/40195445): Move this logging to controller unittest as well.
  // Right now the view is created but not injected. Need to change this when
  // moving this logging.
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.OtpInputDialog.", GetCardType(), ".",
                    GetOtpAuthType(), ".Shown"}),
      true, 1);
}

// Ensures closing tab while dialog being visible is correctly handled.
IN_PROC_BROWSER_TEST_P(CardUnmaskOtpInputDialogBrowserTest,
                       CanCloseTabWhileDialogShowing) {
  ShowUi("");
  VerifyUi();
  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures closing browser while dialog being visible is correctly handled.
IN_PROC_BROWSER_TEST_P(CardUnmaskOtpInputDialogBrowserTest,
                       CanCloseBrowserWhileDialogShowing) {
  ShowUi("");
  VerifyUi();
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures activating the new code link sets it to invalid for a set period of
// time.
#if BUILDFLAG(IS_WIN)
// Triggering logic required for Windows OS runs: https://crbug.com/1254686
#define MAYBE_LinkInvalidatesOnActivation DISABLED_LinkInvalidatesOnActivation
#else
#define MAYBE_LinkInvalidatesOnActivation LinkInvalidatesOnActivation
#endif
IN_PROC_BROWSER_TEST_P(CardUnmaskOtpInputDialogBrowserTest,
                       MAYBE_LinkInvalidatesOnActivation) {
  ShowUi("");
  VerifyUi();
  // Link should be disabled on click.
  GetDialog()->OnNewCodeLinkClicked();
  EXPECT_FALSE(GetDialog()->NewCodeLinkIsEnabledForTesting());
  base::RunLoop run_loop;
  // Link should be re-enabled after timeout completes.
  base::RepeatingClosure
      closure_to_run_after_new_code_link_is_enabled_for_testing =
          run_loop.QuitClosure();
  GetDialog()->SetClosureToRunAfterNewCodeLinkIsEnabledForTesting(
      closure_to_run_after_new_code_link_is_enabled_for_testing);
  run_loop.Run();
  EXPECT_TRUE(GetDialog()->NewCodeLinkIsEnabledForTesting());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CardUnmaskOtpInputDialogBrowserTest,
    testing::ValuesIn({
        std::make_tuple(CardUnmaskChallengeOptionType::kSmsOtp,
                        CreditCard::RecordType::kVirtualCard),
        std::make_tuple(CardUnmaskChallengeOptionType::kSmsOtp,
                        CreditCard::RecordType::kMaskedServerCard),
        std::make_tuple(CardUnmaskChallengeOptionType::kEmailOtp,
                        CreditCard::RecordType::kVirtualCard),
    }));

}  // namespace
}  // namespace autofill
