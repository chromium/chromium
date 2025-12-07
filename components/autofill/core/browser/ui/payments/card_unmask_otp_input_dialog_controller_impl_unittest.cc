// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller_impl.h"

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/otp_unmask_result.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_view.h"
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
  base::WeakPtr<CardUnmaskOtpInputDialogView> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestCardUnmaskOtpInputDialogView> weak_ptr_factory_{
      this};
};

class CardUnmaskOtpInputDialogControllerImplTest
    : public testing::Test,
      public testing::WithParamInterface<
          std::tuple<CardUnmaskChallengeOptionType, CreditCard::RecordType>> {
 public:
  CardUnmaskOtpInputDialogControllerImplTest() = default;
  CardUnmaskOtpInputDialogControllerImplTest(
      const CardUnmaskOtpInputDialogControllerImplTest&) = delete;
  CardUnmaskOtpInputDialogControllerImplTest& operator=(
      const CardUnmaskOtpInputDialogControllerImplTest&) = delete;
  ~CardUnmaskOtpInputDialogControllerImplTest() = default;

  void ShowDialog() {
    CardUnmaskChallengeOption challenge_option;
    challenge_option.type = std::get<0>(GetParam());
    CreditCard::RecordType card_type = std::get<1>(GetParam());
    controller_ = std::make_unique<CardUnmaskOtpInputDialogControllerImpl>(
        card_type, challenge_option, /*delegate=*/nullptr);
    controller_->ShowDialog(base::BindOnce(
        &CardUnmaskOtpInputDialogControllerImplTest::CreateOtpInputDialogView,
        base::Unretained(this)));
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

  base::WeakPtr<CardUnmaskOtpInputDialogView> CreateOtpInputDialogView() {
    dialog_view_ = std::make_unique<TestCardUnmaskOtpInputDialogView>();
    return dialog_view_->GetWeakPtr();
  }

  CardUnmaskOtpInputDialogControllerImpl* controller() const {
    return controller_.get();
  }

 private:
  std::unique_ptr<TestCardUnmaskOtpInputDialogView> dialog_view_;
  std::unique_ptr<CardUnmaskOtpInputDialogControllerImpl> controller_;
};

TEST_P(CardUnmaskOtpInputDialogControllerImplTest,
       DialogCancelledByUserBeforeConfirmation_NoTemporaryError) {
  base::HistogramTester histogram_tester;

  ShowDialog();
  CHECK(controller());
  controller()->OnDialogClosed(/*user_closed_dialog=*/true,
                               /*server_request_succeeded=*/false);

  std::string base_histogram_name = base::StrCat(
      {"Autofill.OtpInputDialog.", GetCardType(), ".", GetOtpAuthType()});
  histogram_tester.ExpectUniqueSample(
      base_histogram_name + ".Result",
      autofill_metrics::OtpInputDialogResult::
          kDialogCancelledByUserBeforeConfirmation,
      1);
  histogram_tester.ExpectUniqueSample(
      base_histogram_name + ".Result.WithNoTemporaryError",
      autofill_metrics::OtpInputDialogResult::
          kDialogCancelledByUserBeforeConfirmation,
      1);
}

TEST_P(CardUnmaskOtpInputDialogControllerImplTest,
       DialogCancelledByUserBeforeConfirmation_OtpMistmatch) {
  base::HistogramTester histogram_tester;

  ShowDialog();
  DCHECK(controller());
  controller()->OnOtpVerificationResult(OtpUnmaskResult::kOtpMismatch);
  controller()->OnDialogClosed(/*user_closed_dialog=*/true,
                               /*server_request_succeeded=*/false);

  std::string base_histogram_name = base::StrCat(
      {"Autofill.OtpInputDialog.", GetCardType(), ".", GetOtpAuthType()});
  histogram_tester.ExpectUniqueSample(
      base_histogram_name + ".ErrorMessageShown",
      autofill_metrics::OtpInputDialogError::kOtpMismatchError, 1);
  histogram_tester.ExpectUniqueSample(
      base_histogram_name + ".Result",
      autofill_metrics::OtpInputDialogResult::
          kDialogCancelledByUserBeforeConfirmation,
      1);
  histogram_tester.ExpectUniqueSample(
      base_histogram_name + ".Result.WithPreviousTemporaryError",
      autofill_metrics::OtpInputDialogResult::
          kDialogCancelledByUserBeforeConfirmation,
      1);
}

TEST_P(CardUnmaskOtpInputDialogControllerImplTest,
       DialogCancelledByUserAfterConfirmation_OtpExpired) {
  base::HistogramTester histogram_tester;

  ShowDialog();
  DCHECK(controller());
  controller()->OnOkButtonClicked(/*otp=*/u"123456");
  controller()->OnOtpVerificationResult(OtpUnmaskResult::kOtpExpired);
  controller()->OnDialogClosed(/*user_closed_dialog=*/true,
                               /*server_request_succeeded=*/false);

  std::string base_histogram_name = base::StrCat(
      {"Autofill.OtpInputDialog.", GetCardType(), ".", GetOtpAuthType()});
  histogram_tester.ExpectUniqueSample(
      base_histogram_name + ".ErrorMessageShown",
      autofill_metrics::OtpInputDialogError::kOtpExpiredError, 1);
  histogram_tester.ExpectUniqueSample(
      base_histogram_name + ".Result",
      autofill_metrics::OtpInputDialogResult::
          kDialogCancelledByUserAfterConfirmation,
      1);
  histogram_tester.ExpectUniqueSample(
      base_histogram_name + ".Result.WithPreviousTemporaryError",
      autofill_metrics::OtpInputDialogResult::
          kDialogCancelledByUserAfterConfirmation,
      1);
}

TEST_P(CardUnmaskOtpInputDialogControllerImplTest, ServerRequestSucceeded) {
  base::HistogramTester histogram_tester;

  ShowDialog();
  DCHECK(controller());
  controller()->OnDialogClosed(/*user_closed_dialog=*/false,
                               /*server_request_succeeded=*/true);

  std::string base_histogram_name = base::StrCat(
      {"Autofill.OtpInputDialog.", GetCardType(), ".", GetOtpAuthType()});
  histogram_tester.ExpectUniqueSample(
      base_histogram_name + ".Result",
      autofill_metrics::OtpInputDialogResult::
          kDialogClosedAfterVerificationSucceeded,
      1);
  histogram_tester.ExpectUniqueSample(
      base_histogram_name + ".Result.WithNoTemporaryError",
      autofill_metrics::OtpInputDialogResult::
          kDialogClosedAfterVerificationSucceeded,
      1);
}

TEST_P(CardUnmaskOtpInputDialogControllerImplTest, ServerRequestFailed) {
  base::HistogramTester histogram_tester;

  ShowDialog();
  DCHECK(controller());
  controller()->OnDialogClosed(/*user_closed_dialog=*/false,
                               /*server_request_succeeded=*/false);

  std::string base_histogram_name = base::StrCat(
      {"Autofill.OtpInputDialog.", GetCardType(), ".", GetOtpAuthType()});
  histogram_tester.ExpectUniqueSample(base_histogram_name + ".Result",
                                      autofill_metrics::OtpInputDialogResult::
                                          kDialogClosedAfterVerificationFailed,
                                      1);
  histogram_tester.ExpectUniqueSample(
      base_histogram_name + ".Result.WithNoTemporaryError",
      autofill_metrics::OtpInputDialogResult::
          kDialogClosedAfterVerificationFailed,
      1);
}

TEST_P(CardUnmaskOtpInputDialogControllerImplTest, NewCodeLinkClicked) {
  base::HistogramTester histogram_tester;

  ShowDialog();
  DCHECK(controller());
  controller()->OnNewCodeLinkClicked();

  std::string base_histogram_name = base::StrCat(
      {"Autofill.OtpInputDialog.", GetCardType(), ".", GetOtpAuthType()});
  histogram_tester.ExpectUniqueSample(base_histogram_name + ".NewOtpRequested",
                                      true, 1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    CardUnmaskOtpInputDialogControllerImplTest,
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
