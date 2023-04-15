// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/card_unmask_otp_input_dialog_controller_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"
#include "components/autofill/core/browser/payments/otp_unmask_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class TestCardUnmaskOtpInputDialogView : public CardUnmaskOtpInputDialogView {
 public:
  TestCardUnmaskOtpInputDialogView() = default;
  ~TestCardUnmaskOtpInputDialogView() override = default;
  void ShowPendingState() override {}
  void ShowInvalidState(const std::u16string& invalid_label_text) override {}
  void Dismiss(bool show_confirmation_before_closing,
               bool user_closed_dialog) override {}
};

class TestCardUnmaskOtpInputDialogControllerImpl
    : public CardUnmaskOtpInputDialogControllerImpl {
 public:
  static void CreateForTesting(content::WebContents* web_contents) {
    web_contents->SetUserData(
        UserDataKey(),
        std::make_unique<TestCardUnmaskOtpInputDialogControllerImpl>(
            web_contents));
  }

  explicit TestCardUnmaskOtpInputDialogControllerImpl(
      content::WebContents* web_contents)
      : CardUnmaskOtpInputDialogControllerImpl(web_contents) {}

  void ShowDialog(TestCardUnmaskOtpInputDialogView* dialog_view,
                  CardUnmaskChallengeOptionType challenge_type) {
    dialog_view_ = dialog_view;
    challenge_type_ = challenge_type;
  }
};

}  // namespace

class CardUnmaskOtpInputDialogControllerImplTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<CardUnmaskChallengeOptionType> {
 public:
  CardUnmaskOtpInputDialogControllerImplTest() = default;
  CardUnmaskOtpInputDialogControllerImplTest(
      const CardUnmaskOtpInputDialogControllerImplTest&) = delete;
  CardUnmaskOtpInputDialogControllerImplTest& operator=(
      const CardUnmaskOtpInputDialogControllerImplTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    TestCardUnmaskOtpInputDialogControllerImpl::CreateForTesting(
        web_contents());
  }

  TestCardUnmaskOtpInputDialogControllerImpl* controller() {
    return static_cast<TestCardUnmaskOtpInputDialogControllerImpl*>(
        TestCardUnmaskOtpInputDialogControllerImpl::FromWebContents(
            web_contents()));
  }

  TestCardUnmaskOtpInputDialogView* test_dialog() { return test_dialog_.get(); }

  CardUnmaskChallengeOptionType GetChallengeOptionType() { return GetParam(); }

  std::string GetOtpAuthType() {
    return autofill_metrics::GetOtpAuthType(GetChallengeOptionType());
  }

 private:
  std::unique_ptr<TestCardUnmaskOtpInputDialogView> test_dialog_ =
      std::make_unique<TestCardUnmaskOtpInputDialogView>();
};

TEST_P(CardUnmaskOtpInputDialogControllerImplTest,
       DialogCancelledByUserBeforeConfirmation_NoTemporaryError) {
  base::HistogramTester histogram_tester;

  DCHECK(controller());
  controller()->ShowDialog(test_dialog(), GetChallengeOptionType());
  controller()->OnDialogClosed(/*user_closed_dialog=*/true,
                               /*server_request_succeeded=*/false);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog." + GetOtpAuthType() + ".Result",
      autofill_metrics::OtpInputDialogResult::
          kDialogCancelledByUserBeforeConfirmation,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog." + GetOtpAuthType() +
          ".Result.WithNoTemporaryError",
      autofill_metrics::OtpInputDialogResult::
          kDialogCancelledByUserBeforeConfirmation,
      1);
}

TEST_P(CardUnmaskOtpInputDialogControllerImplTest,
       DialogCancelledByUserBeforeConfirmation_OtpMistmatch) {
  base::HistogramTester histogram_tester;

  DCHECK(controller());
  controller()->ShowDialog(test_dialog(), GetChallengeOptionType());
  controller()->OnOtpVerificationResult(OtpUnmaskResult::kOtpMismatch);
  controller()->OnDialogClosed(/*user_closed_dialog=*/true,
                               /*server_request_succeeded=*/false);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog." + GetOtpAuthType() + ".ErrorMessageShown",
      autofill_metrics::OtpInputDialogError::kOtpMismatchError, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog." + GetOtpAuthType() + ".Result",
      autofill_metrics::OtpInputDialogResult::
          kDialogCancelledByUserBeforeConfirmation,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog." + GetOtpAuthType() +
          ".Result.WithPreviousTemporaryError",
      autofill_metrics::OtpInputDialogResult::
          kDialogCancelledByUserBeforeConfirmation,
      1);
}

TEST_P(CardUnmaskOtpInputDialogControllerImplTest,
       DialogCancelledByUserAfterConfirmation_OtpExpired) {
  base::HistogramTester histogram_tester;

  DCHECK(controller());
  controller()->ShowDialog(test_dialog(), GetChallengeOptionType());
  controller()->OnOkButtonClicked(/*otp=*/u"123456");
  controller()->OnOtpVerificationResult(OtpUnmaskResult::kOtpExpired);
  controller()->OnDialogClosed(/*user_closed_dialog=*/true,
                               /*server_request_succeeded=*/false);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog." + GetOtpAuthType() + ".ErrorMessageShown",
      autofill_metrics::OtpInputDialogError::kOtpExpiredError, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog." + GetOtpAuthType() + ".Result",
      autofill_metrics::OtpInputDialogResult::
          kDialogCancelledByUserAfterConfirmation,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog." + GetOtpAuthType() +
          ".Result.WithPreviousTemporaryError",
      autofill_metrics::OtpInputDialogResult::
          kDialogCancelledByUserAfterConfirmation,
      1);
}

TEST_P(CardUnmaskOtpInputDialogControllerImplTest, ServerRequestSucceeded) {
  base::HistogramTester histogram_tester;

  DCHECK(controller());
  controller()->ShowDialog(test_dialog(), GetChallengeOptionType());
  controller()->OnDialogClosed(/*user_closed_dialog=*/false,
                               /*server_request_succeeded=*/true);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog." + GetOtpAuthType() + ".Result",
      autofill_metrics::OtpInputDialogResult::
          kDialogClosedAfterVerificationSucceeded,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog." + GetOtpAuthType() +
          ".Result.WithNoTemporaryError",
      autofill_metrics::OtpInputDialogResult::
          kDialogClosedAfterVerificationSucceeded,
      1);
}

TEST_P(CardUnmaskOtpInputDialogControllerImplTest, ServerRequestFailed) {
  base::HistogramTester histogram_tester;

  DCHECK(controller());
  controller()->ShowDialog(test_dialog(), GetChallengeOptionType());
  controller()->OnDialogClosed(/*user_closed_dialog=*/false,
                               /*server_request_succeeded=*/false);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog." + GetOtpAuthType() + ".Result",
      autofill_metrics::OtpInputDialogResult::
          kDialogClosedAfterVerificationFailed,
      1);
  histogram_tester.ExpectUniqueSample("Autofill.OtpInputDialog." +
                                          GetOtpAuthType() +
                                          ".Result.WithNoTemporaryError",
                                      autofill_metrics::OtpInputDialogResult::
                                          kDialogClosedAfterVerificationFailed,
                                      1);
}

TEST_P(CardUnmaskOtpInputDialogControllerImplTest, NewCodeLinkClicked) {
  base::HistogramTester histogram_tester;

  DCHECK(controller());
  controller()->ShowDialog(test_dialog(), GetChallengeOptionType());
  controller()->OnNewCodeLinkClicked();

  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog." + GetOtpAuthType() + ".NewOtpRequested", true,
      1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    CardUnmaskOtpInputDialogControllerImplTest,
    testing::Values(CardUnmaskChallengeOptionType::kSmsOtp,
                    CardUnmaskChallengeOptionType::kEmailOtp));

}  // namespace autofill
