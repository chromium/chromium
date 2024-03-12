// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog_controller_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"

namespace autofill {

class CardUnmaskAuthenticationSelectionDialogControllerImplTest
    : public testing::Test {
 public:
  CardUnmaskAuthenticationSelectionDialogControllerImplTest() = default;
  CardUnmaskAuthenticationSelectionDialogControllerImplTest(
      const CardUnmaskAuthenticationSelectionDialogControllerImplTest&) =
      delete;
  CardUnmaskAuthenticationSelectionDialogControllerImplTest& operator=(
      const CardUnmaskAuthenticationSelectionDialogControllerImplTest&) =
      delete;

  void SetUp() override {
    card_unmask_authentication_selection_dialog_controller_ =
        std::make_unique<CardUnmaskAuthenticationSelectionDialogControllerImpl>(
            /*challenge_options=*/test::GetCardUnmaskChallengeOptions(
                {CardUnmaskChallengeOptionType::
                     kSmsOtp}),  // `challenge_options` must not be empty.
            /*confirm_unmasking_method_callback=*/base::DoNothing(),
            /*cancel_unmasking_callback=*/base::DoNothing());
  }

  CardUnmaskAuthenticationSelectionDialogControllerImpl* controller() {
    return card_unmask_authentication_selection_dialog_controller_.get();
  }

 private:
  std::unique_ptr<CardUnmaskAuthenticationSelectionDialogControllerImpl>
      card_unmask_authentication_selection_dialog_controller_;
};

TEST_F(CardUnmaskAuthenticationSelectionDialogControllerImplTest,
       DialogCanceledByUserBeforeConfirmation) {
  base::HistogramTester histogram_tester;

  controller()->SetSelectedChallengeOptionsForTesting(
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp,
           CardUnmaskChallengeOptionType::kEmailOtp,
           CardUnmaskChallengeOptionType::kCvc}));
  int count = 0;
  for (CardUnmaskChallengeOption challenge_option :
       controller()->GetChallengeOptions()) {
    SCOPED_TRACE(testing::Message() << " count=" << count);
    controller()->SetSelectedChallengeOptionId(
        CardUnmaskChallengeOption::ChallengeOptionId(
            challenge_option.id.value()));
    EXPECT_EQ(challenge_option.id.value(),
              controller()->GetSelectedChallengeOptionIdForTesting().value());
    controller()->OnDialogClosed(/*user_closed_dialog=*/true,
                                 /*server_success=*/false);
    count++;
    histogram_tester.ExpectUniqueSample(
        "Autofill.CardUnmaskAuthenticationSelectionDialog.Result",
        AutofillMetrics::CardUnmaskAuthenticationSelectionDialogResultMetric::
            kCanceledByUserBeforeSelection,
        count);
  }
}

TEST_F(CardUnmaskAuthenticationSelectionDialogControllerImplTest,
       DialogCanceledByUserAfterConfirmation) {
  base::HistogramTester histogram_tester;

  controller()->SetSelectedChallengeOptionsForTesting(
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp,
           CardUnmaskChallengeOptionType::kEmailOtp,
           CardUnmaskChallengeOptionType::kCvc}));
  int count = 0;
  for (CardUnmaskChallengeOption challenge_option :
       controller()->GetChallengeOptions()) {
    SCOPED_TRACE(testing::Message() << " count=" << count);
    controller()->SetSelectedChallengeOptionId(
        CardUnmaskChallengeOption::ChallengeOptionId(
            challenge_option.id.value()));
    EXPECT_EQ(challenge_option.id.value(),
              controller()->GetSelectedChallengeOptionIdForTesting().value());

    controller()->OnOkButtonClicked();
    controller()->OnDialogClosed(/*user_closed_dialog=*/true,
                                 /*server_success=*/false);
    count++;
    histogram_tester.ExpectUniqueSample(
        "Autofill.CardUnmaskAuthenticationSelectionDialog.Result",
        AutofillMetrics::CardUnmaskAuthenticationSelectionDialogResultMetric::
            kCanceledByUserAfterSelection,
        count);
  }
}

TEST_F(CardUnmaskAuthenticationSelectionDialogControllerImplTest,
       ServerRequestSucceeded) {
  base::HistogramTester histogram_tester;
  int count = 0;
  for (CardUnmaskChallengeOption challenge_option :
       controller()->GetChallengeOptions()) {
    SCOPED_TRACE(testing::Message() << " count=" << count);
    controller()->SetSelectedChallengeOptionsForTesting(
        test::GetCardUnmaskChallengeOptions(
            {CardUnmaskChallengeOptionType::kSmsOtp,
             CardUnmaskChallengeOptionType::kEmailOtp}));

    controller()->SetSelectedChallengeOptionId(
        CardUnmaskChallengeOption::ChallengeOptionId(
            challenge_option.id.value()));
    EXPECT_EQ(challenge_option.id.value(),
              controller()->GetSelectedChallengeOptionIdForTesting().value());

    controller()->OnOkButtonClicked();
    controller()->OnDialogClosed(/*user_closed_dialog=*/false,
                                 /*server_success=*/true);
    count++;
    histogram_tester.ExpectUniqueSample(
        "Autofill.CardUnmaskAuthenticationSelectionDialog.Result",
        AutofillMetrics::CardUnmaskAuthenticationSelectionDialogResultMetric::
            kDismissedByServerRequestSuccess,
        count);
  }
}

TEST_F(CardUnmaskAuthenticationSelectionDialogControllerImplTest,
       ServerRequestFailed) {
  base::HistogramTester histogram_tester;

  controller()->SetSelectedChallengeOptionsForTesting(
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp,
           CardUnmaskChallengeOptionType::kEmailOtp}));
  int count = 0;
  for (CardUnmaskChallengeOption challenge_option :
       controller()->GetChallengeOptions()) {
    SCOPED_TRACE(testing::Message() << " count=" << count);
    controller()->SetSelectedChallengeOptionId(
        CardUnmaskChallengeOption::ChallengeOptionId(
            challenge_option.id.value()));
    EXPECT_EQ(challenge_option.id.value(),
              controller()->GetSelectedChallengeOptionIdForTesting().value());

    controller()->OnOkButtonClicked();
    controller()->OnDialogClosed(/*user_closed_dialog=*/false,
                                 /*server_success=*/false);
    count++;
    histogram_tester.ExpectUniqueSample(
        "Autofill.CardUnmaskAuthenticationSelectionDialog.Result",
        AutofillMetrics::CardUnmaskAuthenticationSelectionDialogResultMetric::
            kDismissedByServerRequestFailure,
        count);
  }
}

TEST_F(CardUnmaskAuthenticationSelectionDialogControllerImplTest,
       AcceptedNoServerRequestNecessary) {
  base::HistogramTester histogram_tester;

  controller()->SetSelectedChallengeOptionsForTesting(
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kCvc}));
  controller()->SetSelectedChallengeOptionId(
      controller()->GetChallengeOptions()[0].id);
  EXPECT_EQ(controller()->GetChallengeOptions()[0].id,
            controller()->GetSelectedChallengeOptionIdForTesting());

  controller()->OnOkButtonClicked();
  controller()->OnDialogClosed(/*user_closed_dialog=*/false,
                               /*server_success=*/false);

  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Result",
      AutofillMetrics::CardUnmaskAuthenticationSelectionDialogResultMetric::
          kDismissedByUserAcceptanceNoServerRequestNeeded,
      1);
}

TEST_F(CardUnmaskAuthenticationSelectionDialogControllerImplTest,
       SelectedCardUnmaskChallengeOptionType) {
  // Ensure the challenge option type is initialized to kUnknown.
  EXPECT_EQ(CardUnmaskChallengeOptionType::kUnknownType,
            controller()->GetSelectedChallengeOptionTypeForTesting());

  controller()->SetSelectedChallengeOptionsForTesting(
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp,
           CardUnmaskChallengeOptionType::kEmailOtp,
           CardUnmaskChallengeOptionType::kCvc}));

  for (CardUnmaskChallengeOption challenge_option :
       controller()->GetChallengeOptions()) {
    controller()->SetSelectedChallengeOptionId(
        CardUnmaskChallengeOption::ChallengeOptionId(
            challenge_option.id.value()));
    controller()->OnOkButtonClicked();
    EXPECT_EQ(challenge_option.type,
              controller()->GetSelectedChallengeOptionTypeForTesting());
  }
}

}  // namespace autofill
