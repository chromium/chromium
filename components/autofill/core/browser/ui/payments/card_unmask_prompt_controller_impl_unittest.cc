// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using base::ASCIIToUTF16;
using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;

class TestCardUnmaskDelegate : public CardUnmaskDelegate {
 public:
  TestCardUnmaskDelegate() = default;

  TestCardUnmaskDelegate(const TestCardUnmaskDelegate&) = delete;
  TestCardUnmaskDelegate& operator=(const TestCardUnmaskDelegate&) = delete;

  virtual ~TestCardUnmaskDelegate() = default;

  // CardUnmaskDelegate implementation.
  void OnUnmaskPromptAccepted(
      const UserProvidedUnmaskDetails& details) override {
    details_ = details;
  }
  void OnUnmaskPromptCancelled() override {}
  bool ShouldOfferFidoAuth() const override { return false; }

  const UserProvidedUnmaskDetails& details() { return details_; }

  base::WeakPtr<TestCardUnmaskDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  UserProvidedUnmaskDetails details_;
  base::WeakPtrFactory<TestCardUnmaskDelegate> weak_factory_{this};
};

class TestCardUnmaskPromptView : public CardUnmaskPromptView {
 public:
  TestCardUnmaskPromptView(CardUnmaskPromptController* controller)
      : controller_(controller) {}
  void Show() override {}
  void Dismiss() override {
    // Notify the controller that the view was dismissed.
    controller_->OnUnmaskDialogClosed();
  }
  void ControllerGone() override { controller_ = nullptr; }
  void DisableAndWaitForVerification() override {}
  void GotVerificationResult(const std::u16string& error_message,
                             bool allow_retry) override {}

 private:
  raw_ptr<CardUnmaskPromptController> controller_ = nullptr;
};

class TestCardUnmaskPromptController : public CardUnmaskPromptControllerImpl {
 public:
  TestCardUnmaskPromptController(
      TestingPrefServiceSimple* pref_service,
      const CreditCard& card,
      const CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<CardUnmaskDelegate> delegate)
      : CardUnmaskPromptControllerImpl(pref_service,
                                       card,
                                       card_unmask_prompt_options,
                                       delegate) {}

  TestCardUnmaskPromptController(const TestCardUnmaskPromptController&) =
      delete;
  TestCardUnmaskPromptController& operator=(
      const TestCardUnmaskPromptController&) = delete;

#if BUILDFLAG(IS_ANDROID)
  bool ShouldOfferWebauthn() const override { return should_offer_webauthn_; }
#endif
  void set_should_offer_webauthn(bool should) {
    should_offer_webauthn_ = should;
  }

  base::WeakPtr<TestCardUnmaskPromptController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  bool should_offer_webauthn_;
  base::WeakPtrFactory<TestCardUnmaskPromptController> weak_factory_{this};
};

class CardUnmaskPromptControllerImplGenericTest : public testing::Test {
 public:
  CardUnmaskPromptControllerImplGenericTest() {
#if BUILDFLAG(IS_ANDROID)
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kAutofillCreditCardFidoAuthOfferCheckboxState, true);
#endif
  }

  CardUnmaskPromptControllerImplGenericTest(
      const CardUnmaskPromptControllerImplGenericTest&) = delete;
  CardUnmaskPromptControllerImplGenericTest& operator=(
      const CardUnmaskPromptControllerImplGenericTest&) = delete;

  // Shows the Card Unmask Prompt. `challenge_option` being present denotes that
  // we are in the virtual card use-case.
  void ShowPrompt(const std::optional<autofill::CardUnmaskChallengeOption>&
                      challenge_option = std::nullopt) {
    card_.set_record_type(challenge_option.has_value()
                              ? CreditCard::RecordType::kVirtualCard
                              : CreditCard::RecordType::kMaskedServerCard);

    CardUnmaskPromptOptions card_unmask_prompt_options =
        CardUnmaskPromptOptions(
            challenge_option,
            payments::PaymentsAutofillClient::UnmaskCardReason::kAutofill);
    controller_ = std::make_unique<TestCardUnmaskPromptController>(
        &pref_service_, card_, card_unmask_prompt_options,
        delegate_.GetWeakPtr());
    controller_->ShowPrompt(base::BindOnce(
        &CardUnmaskPromptControllerImplGenericTest::CreateCardUnmaskPromptView,
        base::Unretained(this)));
  }

  void DismissPrompt() {
    test_unmask_prompt_view_->Dismiss();
    test_unmask_prompt_view_.reset();
  }

  void ShowPromptAndSimulateResponse(bool enable_fido_auth,
                                     bool should_unmask_virtual_card = false,
                                     bool was_checkbox_visible = true) {
    ShowPrompt(should_unmask_virtual_card
                   ? std::optional<autofill::CardUnmaskChallengeOption>(
                         test::GetCardUnmaskChallengeOptions(
                             {CardUnmaskChallengeOptionType::kCvc})[0])
                   : std::nullopt);
    controller_->OnUnmaskPromptAccepted(u"444", u"01", u"2050",
                                        enable_fido_auth, was_checkbox_visible);
  }

 protected:
  void SetCreditCardForTesting(CreditCard card) { card_ = card; }

  TestingPrefServiceSimple pref_service_;
  CreditCard card_ = test::GetMaskedServerCard();
  TestCardUnmaskDelegate delegate_;
  std::unique_ptr<TestCardUnmaskPromptView> test_unmask_prompt_view_;
  std::unique_ptr<TestCardUnmaskPromptController> controller_;

 private:
  CardUnmaskPromptView* CreateCardUnmaskPromptView() {
    test_unmask_prompt_view_ =
        std::make_unique<TestCardUnmaskPromptView>(controller_.get());
    return test_unmask_prompt_view_.get();
  }
};

#if BUILDFLAG(IS_ANDROID)
TEST_F(CardUnmaskPromptControllerImplGenericTest,
       FidoAuthOfferCheckboxStatePersistent) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/true);
  EXPECT_TRUE(pref_service_.GetBoolean(
      prefs::kAutofillCreditCardFidoAuthOfferCheckboxState));

  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false);
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kAutofillCreditCardFidoAuthOfferCheckboxState));
}

TEST_F(CardUnmaskPromptControllerImplGenericTest,
       FidoAuthOfferCheckboxStateUnchangedWhenInvisible) {
  pref_service_.SetBoolean(prefs::kAutofillCreditCardFidoAuthOfferCheckboxState,
                           true);
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false,
                                /*should_unmask_virtual_card=*/false,
                                /*was_checkbox_visible=*/false);
  EXPECT_TRUE(pref_service_.GetBoolean(
      prefs::kAutofillCreditCardFidoAuthOfferCheckboxState));

  pref_service_.SetBoolean(prefs::kAutofillCreditCardFidoAuthOfferCheckboxState,
                           false);
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/true,
                                /*should_unmask_virtual_card=*/false,
                                /*was_checkbox_visible=*/false);
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kAutofillCreditCardFidoAuthOfferCheckboxState));
}

TEST_F(CardUnmaskPromptControllerImplGenericTest,
       PopulateCheckboxToUserProvidedUnmaskDetails) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/true);

  EXPECT_TRUE(delegate_.details().enable_fido_auth);
}
#endif

TEST_F(CardUnmaskPromptControllerImplGenericTest, LogRealPanResultSuccess) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false);
  base::HistogramTester histogram_tester;
  controller_->OnVerificationResult(PaymentsRpcResult::kSuccess);

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.GetRealPanResult.ServerCard",
      AutofillMetrics::PAYMENTS_RESULT_SUCCESS, 1);
}

TEST_F(CardUnmaskPromptControllerImplGenericTest, LogRealPanTryAgainFailure) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false);
  base::HistogramTester histogram_tester;

  controller_->OnVerificationResult(PaymentsRpcResult::kTryAgainFailure);

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.GetRealPanResult.ServerCard",
      AutofillMetrics::PAYMENTS_RESULT_TRY_AGAIN_FAILURE, 1);
}

TEST_F(CardUnmaskPromptControllerImplGenericTest, LogRealPanClientSideTimeout) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false);
  base::HistogramTester histogram_tester;

  controller_->OnVerificationResult(PaymentsRpcResult::kClientSideTimeout);

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.GetRealPanResult.ServerCard",
      AutofillMetrics::PAYMENTS_RESULT_CLIENT_SIDE_TIMEOUT, 1);
}

TEST_F(CardUnmaskPromptControllerImplGenericTest,
       LogUnmaskingDurationResultSuccess) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false);
  base::HistogramTester histogram_tester;

  controller_->OnVerificationResult(PaymentsRpcResult::kSuccess);

  histogram_tester.ExpectTotalCount("Autofill.UnmaskPrompt.UnmaskingDuration",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.UnmaskingDuration.ServerCard.Success", 1);
}

TEST_F(CardUnmaskPromptControllerImplGenericTest,
       LogUnmaskingDurationTryAgainFailure) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false);
  base::HistogramTester histogram_tester;

  controller_->OnVerificationResult(PaymentsRpcResult::kTryAgainFailure);

  histogram_tester.ExpectTotalCount("Autofill.UnmaskPrompt.UnmaskingDuration",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.UnmaskingDuration.ServerCard.Failure", 1);
}

TEST_F(CardUnmaskPromptControllerImplGenericTest,
       LogUnmaskingDurationVcnRetrievalPermanentFailure) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false,
                                /*should_unmask_virtual_card=*/true);
  base::HistogramTester histogram_tester;

  controller_->OnVerificationResult(
      PaymentsRpcResult::kVcnRetrievalPermanentFailure);

  histogram_tester.ExpectTotalCount("Autofill.UnmaskPrompt.UnmaskingDuration",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.UnmaskingDuration.VirtualCard.VcnRetrievalFailure",
      1);
}

TEST_F(CardUnmaskPromptControllerImplGenericTest,
       LogUnmaskingDurationClientSideTimeout) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false);
  base::HistogramTester histogram_tester;

  controller_->OnVerificationResult(PaymentsRpcResult::kClientSideTimeout);

  histogram_tester.ExpectTotalCount("Autofill.UnmaskPrompt.UnmaskingDuration",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.UnmaskingDuration.ServerCard.ClientSideTimeout",
      1);
}

// Tests to ensure the UI text elements are shown correctly in the card unmask
// prompt.
class CardUnmaskPromptTextTest
    : public CardUnmaskPromptControllerImplGenericTest {};

// Ensures the card information is shown correctly.
TEST_F(CardUnmaskPromptTextTest, DisplayCardInformation) {
  ShowPrompt();
#if BUILDFLAG(IS_IOS)
  EXPECT_TRUE(controller_->GetInstructionsMessage().find(
                  card_.CardNameAndLastFourDigits()) == std::string::npos);
#elif BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(controller_->GetCardName(), card_.CardNameForAutofillDisplay());
  EXPECT_EQ(controller_->GetCardLastFourDigits(),
            card_.ObfuscatedNumberWithVisibleLastFourDigits());
#else
  EXPECT_TRUE(controller_->GetWindowTitle().find(
                  card_.CardNameAndLastFourDigits()) != std::string::npos);
#endif
}

// Tests the title and instructions message in the credit card unmask dialog.
TEST_F(CardUnmaskPromptTextTest, TitleAndInstructionMessage) {
  ShowPrompt();
#if BUILDFLAG(IS_IOS)
  EXPECT_EQ(controller_->GetNavigationTitle(), u"Verification");
  EXPECT_EQ(controller_->GetWindowTitle(), u"Enter your CVC");
  EXPECT_EQ(controller_->GetInstructionsMessage(),
            u"To help keep your card secure, enter the CVC on the back of "
            u"your card");
#else

#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(controller_->GetWindowTitle(), u"Enter your CVC");
#else
  EXPECT_EQ(controller_->GetWindowTitle(),
            u"Enter the CVC for " + card_.CardNameAndLastFourDigits());
#endif

  // On Desktop/Android, if the issuer is not Amex, the instructions message
  // prompts users to enter the CVC located on the back of the card.
  EXPECT_EQ(
      controller_->GetInstructionsMessage(),
      u"To help keep your card secure, enter the CVC on the back of your card");

#endif
  DismissPrompt();
}

TEST_F(CardUnmaskPromptTextTest, TitleAndInstructionMessageAmex) {
  // On Amex cards, the CVC is present on the front of the card. Test that the
  // dialog relays this information to the users.
  card_ = test::GetMaskedServerCardAmex();
  ShowPrompt();
#if BUILDFLAG(IS_IOS)
  EXPECT_EQ(controller_->GetNavigationTitle(), u"Verification");
  EXPECT_EQ(controller_->GetWindowTitle(), u"Enter your CVC");
  EXPECT_EQ(controller_->GetInstructionsMessage(),
            u"To help keep your card secure, enter the CVC on the front of "
            u"your card");
#else

#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(controller_->GetWindowTitle(), u"Enter your CVC");
#else
  EXPECT_EQ(controller_->GetWindowTitle(),
            u"Enter the CVC for " + card_.CardNameAndLastFourDigits());
#endif

  // On Desktop/Android, if the issuer is Amex, the instructions message prompts
  // users to enter the CVC located on the front of the card.
  EXPECT_EQ(controller_->GetInstructionsMessage(),
            u"To help keep your card secure, enter the CVC on the front of "
            u"your card");

#endif
  DismissPrompt();
}

// Tests the title and instructions message in the credit card unmask dialog for
// expired cards.
TEST_F(CardUnmaskPromptTextTest, ExpiredCardTitleAndInstructionMessage) {
  card_ = test::GetExpiredCreditCard();
  ShowPrompt();
#if BUILDFLAG(IS_IOS)
  EXPECT_EQ(controller_->GetNavigationTitle(), u"Verification");
  EXPECT_EQ(controller_->GetWindowTitle(), u"Card expired");
  EXPECT_EQ(controller_->GetInstructionsMessage(),
            u"Enter your new expiration date and CVC on the back of your card");

#else

#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(controller_->GetWindowTitle(), u"Card expired");
#else
  EXPECT_EQ(controller_->GetWindowTitle(),
            u"Enter the expiration date and CVC for " +
                card_.CardNameAndLastFourDigits());
#endif

  EXPECT_EQ(controller_->GetInstructionsMessage(),
            u"Enter your new expiration date and CVC on the back of your card");

#endif
  DismissPrompt();
}

// Ensures the instruction message and window title is correctly displayed when
// showing the card unmask prompt for a virtual card. This test also checks that
// the expected CVC length is correctly set for virtual cards. Virtual cards are
// not currently supported on iOS, so we don't test on the platform.
TEST_F(CardUnmaskPromptTextTest,
       ChallengeOptionInstructionMessageAndWindowTitleAndExpectedCvcLength) {
  // Test that if the network is not American Express and the challenge option
  // denotes that the security code is on the back of the card, its expected
  // length is 3.
  card_.set_record_type(CreditCard::RecordType::kVirtualCard);
  ShowPrompt(test::GetCardUnmaskChallengeOptions(
      {CardUnmaskChallengeOptionType::kCvc})[0]);
  EXPECT_EQ(controller_->GetInstructionsMessage(),
            u"Enter the 3-digit security code on the back of your card so your "
            u"bank can verify it's you");
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_EQ(controller_->GetWindowTitle(), u"Enter your security code");
#else
  EXPECT_EQ(
      controller_->GetWindowTitle(),
      u"Enter your security code for " + card_.CardNameAndLastFourDigits());
#endif
  EXPECT_EQ(controller_->GetExpectedCvcLength(), 3);
  DismissPrompt();
}

TEST_F(
    CardUnmaskPromptTextTest,
    ChallengeOptionInstructionMessageAndWindowTitleAndExpectedCvcLengthAmex) {
  // Test that if the network is American Express and the challenge option
  // denotes that the security code is on the back of the card, its expected
  // length is still 3.
  card_ = test::GetMaskedServerCardAmex();
  card_.set_record_type(CreditCard::RecordType::kVirtualCard);
  ShowPrompt(test::GetCardUnmaskChallengeOptions(
      {CardUnmaskChallengeOptionType::kCvc})[0]);
  EXPECT_EQ(controller_->GetInstructionsMessage(),
            u"Enter the 3-digit security code on the back of your card so your "
            u"bank can verify it's you");
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_EQ(controller_->GetWindowTitle(), u"Enter your security code");
#else
  EXPECT_EQ(
      controller_->GetWindowTitle(),
      u"Enter your security code for " + card_.CardNameAndLastFourDigits());
#endif
  EXPECT_EQ(controller_->GetExpectedCvcLength(), 3);
  DismissPrompt();
}

class LoggingValidationTestForNickname
    : public CardUnmaskPromptControllerImplGenericTest,
      public testing::WithParamInterface<bool> {
 public:
  LoggingValidationTestForNickname() : card_has_nickname_(GetParam()) {}

  LoggingValidationTestForNickname(const LoggingValidationTestForNickname&) =
      delete;
  LoggingValidationTestForNickname& operator=(
      const LoggingValidationTestForNickname&) = delete;

  ~LoggingValidationTestForNickname() override = default;

  void SetUp() override {
    CardUnmaskPromptControllerImplGenericTest::SetUp();
    SetCreditCardForTesting(card_has_nickname_
                                ? test::GetMaskedServerCardWithNickname()
                                : test::GetMaskedServerCard());
  }

  bool card_has_nickname() { return card_has_nickname_; }

 private:
  bool card_has_nickname_;
};

TEST_P(LoggingValidationTestForNickname,
       MaskedServerCard_LogUnmaskPromptShown) {
  base::HistogramTester histogram_tester;
  ShowPrompt();

  histogram_tester.ExpectUniqueSample("Autofill.UnmaskPrompt.ServerCard.Events",
                                      AutofillMetrics::UNMASK_PROMPT_SHOWN, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.UnmaskPrompt.Events.WithNickname",
      AutofillMetrics::UNMASK_PROMPT_SHOWN, card_has_nickname() ? 1 : 0);
}

TEST_P(LoggingValidationTestForNickname, VirtualCard_LogUnmaskPromptShown) {
  base::HistogramTester histogram_tester;
  ShowPrompt(std::optional<autofill::CardUnmaskChallengeOption>(
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kCvc})[0]));

  histogram_tester.ExpectUniqueSample(
      "Autofill.UnmaskPrompt.VirtualCard.Events",
      AutofillMetrics::UNMASK_PROMPT_SHOWN, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.UnmaskPrompt.Events.WithNickname",
      AutofillMetrics::UNMASK_PROMPT_SHOWN, card_has_nickname() ? 1 : 0);
}

TEST_P(LoggingValidationTestForNickname, LogClosedNoAttempts) {
  ShowPrompt();
  base::HistogramTester histogram_tester;
  DismissPrompt();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.ServerCard.Events",
      AutofillMetrics::UNMASK_PROMPT_CLOSED_NO_ATTEMPTS, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events.WithNickname",
      AutofillMetrics::UNMASK_PROMPT_CLOSED_NO_ATTEMPTS,
      card_has_nickname() ? 1 : 0);
}

TEST_P(LoggingValidationTestForNickname, LogClosedAbandonUnmasking) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false);
  base::HistogramTester histogram_tester;

  DismissPrompt();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.ServerCard.Events",
      AutofillMetrics::UNMASK_PROMPT_CLOSED_ABANDON_UNMASKING, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events.WithNickname",
      AutofillMetrics::UNMASK_PROMPT_CLOSED_ABANDON_UNMASKING,
      card_has_nickname() ? 1 : 0);
}

TEST_P(LoggingValidationTestForNickname, LogClosedFailedToUnmaskRetriable) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false);
  controller_->OnVerificationResult(PaymentsRpcResult::kTryAgainFailure);
  base::HistogramTester histogram_tester;

  EXPECT_EQ(PaymentsRpcResult::kTryAgainFailure,
            controller_->GetVerificationResult());
  DismissPrompt();
  // State should be cleared when the dialog is closed.
  EXPECT_EQ(PaymentsRpcResult::kNone, controller_->GetVerificationResult());

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.ServerCard.Events",
      AutofillMetrics ::UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_RETRIABLE_FAILURE,
      1);
  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events.WithNickname",
      AutofillMetrics ::UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_RETRIABLE_FAILURE,
      card_has_nickname() ? 1 : 0);
}

TEST_P(LoggingValidationTestForNickname, LogClosedFailedToUnmaskNonRetriable) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false);
  controller_->OnVerificationResult(PaymentsRpcResult::kPermanentFailure);
  base::HistogramTester histogram_tester;

  EXPECT_EQ(PaymentsRpcResult::kPermanentFailure,
            controller_->GetVerificationResult());
  DismissPrompt();
  // State should be cleared when the dialog is closed.
  EXPECT_EQ(PaymentsRpcResult::kNone, controller_->GetVerificationResult());

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.ServerCard.Events",
      AutofillMetrics ::
          UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_NON_RETRIABLE_FAILURE,
      1);
  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events.WithNickname",
      AutofillMetrics ::
          UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_NON_RETRIABLE_FAILURE,
      card_has_nickname() ? 1 : 0);
}

TEST_P(LoggingValidationTestForNickname, LogUnmaskedCardFirstAttempt) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false);
  base::HistogramTester histogram_tester;

  controller_->OnVerificationResult(PaymentsRpcResult::kSuccess);

  EXPECT_EQ(PaymentsRpcResult::kSuccess, controller_->GetVerificationResult());
  DismissPrompt();
  // State should be cleared when the dialog is closed.
  EXPECT_EQ(PaymentsRpcResult::kNone, controller_->GetVerificationResult());

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.ServerCard.Events",
      AutofillMetrics::UNMASK_PROMPT_UNMASKED_CARD_FIRST_ATTEMPT, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events.WithNickname",
      AutofillMetrics::UNMASK_PROMPT_UNMASKED_CARD_FIRST_ATTEMPT,
      card_has_nickname() ? 1 : 0);
}

TEST_P(LoggingValidationTestForNickname, LogUnmaskedCardAfterFailure) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false);
  controller_->OnVerificationResult(PaymentsRpcResult::kTryAgainFailure);
  controller_->OnUnmaskPromptAccepted(u"444", u"01", u"2050",

                                      /*enable_fido_auth=*/false,
                                      /*was_checkbox_visible=*/true);
  base::HistogramTester histogram_tester;

  controller_->OnVerificationResult(PaymentsRpcResult::kSuccess);
  DismissPrompt();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.ServerCard.Events",
      AutofillMetrics::UNMASK_PROMPT_UNMASKED_CARD_AFTER_FAILED_ATTEMPTS, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events.WithNickname",
      AutofillMetrics::UNMASK_PROMPT_UNMASKED_CARD_AFTER_FAILED_ATTEMPTS,
      card_has_nickname() ? 1 : 0);
}

TEST_P(LoggingValidationTestForNickname, LogDurationNoAttempts) {
  ShowPrompt();
  base::HistogramTester histogram_tester;

  DismissPrompt();

  histogram_tester.ExpectTotalCount("Autofill.UnmaskPrompt.Duration", 1);
  histogram_tester.ExpectTotalCount("Autofill.UnmaskPrompt.Duration.NoAttempts",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.Duration.WithNickname",
      card_has_nickname() ? 1 : 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.Duration.NoAttempts.WithNickname",
      card_has_nickname() ? 1 : 0);
}

TEST_P(LoggingValidationTestForNickname, LogDurationAbandonUnmasking) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false);
  base::HistogramTester histogram_tester;

  DismissPrompt();

  histogram_tester.ExpectTotalCount("Autofill.UnmaskPrompt.Duration", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.Duration.AbandonUnmasking", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.Duration.WithNickname",
      card_has_nickname() ? 1 : 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.Duration.AbandonUnmasking.WithNickname",
      card_has_nickname() ? 1 : 0);
}

TEST_P(LoggingValidationTestForNickname, LogDurationFailedToUnmaskRetriable) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false);
  controller_->OnVerificationResult(PaymentsRpcResult::kTryAgainFailure);
  base::HistogramTester histogram_tester;

  DismissPrompt();

  histogram_tester.ExpectTotalCount("Autofill.UnmaskPrompt.Duration", 1);
  histogram_tester.ExpectTotalCount("Autofill.UnmaskPrompt.Duration.Failure",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.Duration.WithNickname",
      card_has_nickname() ? 1 : 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.Duration.Failure.WithNickname",
      card_has_nickname() ? 1 : 0);
}

TEST_P(LoggingValidationTestForNickname,
       LogDurationFailedToUnmaskNonRetriable) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false);
  controller_->OnVerificationResult(PaymentsRpcResult::kPermanentFailure);
  base::HistogramTester histogram_tester;

  DismissPrompt();

  histogram_tester.ExpectTotalCount("Autofill.UnmaskPrompt.Duration", 1);
  histogram_tester.ExpectTotalCount("Autofill.UnmaskPrompt.Duration.Failure",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.Duration.WithNickname",
      card_has_nickname() ? 1 : 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.Duration.Failure.WithNickname",
      card_has_nickname() ? 1 : 0);
}

TEST_P(LoggingValidationTestForNickname, LogDurationCardFirstAttempt) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false);
  base::HistogramTester histogram_tester;

  controller_->OnVerificationResult(PaymentsRpcResult::kSuccess);
  DismissPrompt();

  histogram_tester.ExpectTotalCount("Autofill.UnmaskPrompt.Duration", 1);
  histogram_tester.ExpectTotalCount("Autofill.UnmaskPrompt.Duration.Success",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.Duration.WithNickname",
      card_has_nickname() ? 1 : 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.Duration.Success.WithNickname",
      card_has_nickname() ? 1 : 0);
}

TEST_P(LoggingValidationTestForNickname, LogDurationUnmaskedCardAfterFailure) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false);
  controller_->OnVerificationResult(PaymentsRpcResult::kTryAgainFailure);
  controller_->OnUnmaskPromptAccepted(u"444", u"01", u"2050",
                                      /*enable_fido_auth=*/false,
                                      /*was_checkbox_visible=*/true);
  base::HistogramTester histogram_tester;

  controller_->OnVerificationResult(PaymentsRpcResult::kSuccess);
  DismissPrompt();

  histogram_tester.ExpectTotalCount("Autofill.UnmaskPrompt.Duration", 1);
  histogram_tester.ExpectTotalCount("Autofill.UnmaskPrompt.Duration.Success",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.Duration.WithNickname",
      card_has_nickname() ? 1 : 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.Duration.Success.WithNickname",
      card_has_nickname() ? 1 : 0);
}

TEST_P(LoggingValidationTestForNickname, LogTimeBeforeAbandonUnmasking) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false);
  base::HistogramTester histogram_tester;

  DismissPrompt();

  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.TimeBeforeAbandonUnmasking", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.UnmaskPrompt.TimeBeforeAbandonUnmasking.WithNickname",
      card_has_nickname() ? 1 : 0);
}

INSTANTIATE_TEST_SUITE_P(CardUnmaskPromptControllerImplGenericTest,
                         LoggingValidationTestForNickname,
                         ::testing::Bool());

struct CvcCase {
  const char* input;
  bool valid;
  // null when |valid| is false.
  const char* canonicalized_input;
};

class CvcInputValidationTest : public CardUnmaskPromptControllerImplGenericTest,
                               public testing::WithParamInterface<CvcCase> {
 public:
  CvcInputValidationTest() = default;

  CvcInputValidationTest(const CvcInputValidationTest&) = delete;
  CvcInputValidationTest& operator=(const CvcInputValidationTest&) = delete;

  ~CvcInputValidationTest() override = default;
};

TEST_P(CvcInputValidationTest, CvcInputValidation) {
  auto cvc_case = GetParam();
  ShowPrompt();
  EXPECT_EQ(cvc_case.valid,
            controller_->InputCvcIsValid(ASCIIToUTF16(cvc_case.input)));
  if (!cvc_case.valid)
    return;

  controller_->OnUnmaskPromptAccepted(
      ASCIIToUTF16(cvc_case.input), u"1", u"2050",
      /*enable_fido_auth=*/false, /*was_checkbox_visible=*/true);
  EXPECT_EQ(ASCIIToUTF16(cvc_case.canonicalized_input),
            delegate_.details().cvc);
}

INSTANTIATE_TEST_SUITE_P(CardUnmaskPromptControllerImplGenericTest,
                         CvcInputValidationTest,
                         testing::Values(CvcCase{"123", true, "123"},
                                         CvcCase{"123 ", true, "123"},
                                         CvcCase{" 1234 ", false},
                                         CvcCase{"IOU", false}));

class CvcInputAmexValidationTest
    : public CardUnmaskPromptControllerImplGenericTest,
      public testing::WithParamInterface<CvcCase> {
 public:
  CvcInputAmexValidationTest() = default;

  CvcInputAmexValidationTest(const CvcInputAmexValidationTest&) = delete;
  CvcInputAmexValidationTest& operator=(const CvcInputAmexValidationTest&) =
      delete;

  ~CvcInputAmexValidationTest() override = default;
};

TEST_P(CvcInputAmexValidationTest, CvcInputValidation) {
  auto cvc_case_amex = GetParam();
  SetCreditCardForTesting(test::GetMaskedServerCardAmex());
  ShowPrompt();
  EXPECT_EQ(cvc_case_amex.valid,
            controller_->InputCvcIsValid(ASCIIToUTF16(cvc_case_amex.input)));
  if (!cvc_case_amex.valid)
    return;

  controller_->OnUnmaskPromptAccepted(
      ASCIIToUTF16(cvc_case_amex.input), std::u16string(), std::u16string(),
      /*enable_fido_auth=*/false, /*was_checkbox_visible=*/true);
  EXPECT_EQ(ASCIIToUTF16(cvc_case_amex.canonicalized_input),
            delegate_.details().cvc);
}

INSTANTIATE_TEST_SUITE_P(CardUnmaskPromptControllerImplGenericTest,
                         CvcInputAmexValidationTest,
                         testing::Values(CvcCase{"123", false},
                                         CvcCase{"123 ", false},
                                         CvcCase{"1234", true, "1234"},
                                         CvcCase{"\t1234 ", true, "1234"},
                                         CvcCase{" 1234", true, "1234"},
                                         CvcCase{"IOU$", false}));

struct ExpirationDateTestCase {
  const char* input_month;
  const char* input_year;
  bool valid;
};

class ExpirationDateValidationTest
    : public CardUnmaskPromptControllerImplGenericTest,
      public testing::WithParamInterface<ExpirationDateTestCase> {
 public:
  ExpirationDateValidationTest() = default;

  ExpirationDateValidationTest(const ExpirationDateValidationTest&) = delete;
  ExpirationDateValidationTest& operator=(const ExpirationDateValidationTest&) =
      delete;

  ~ExpirationDateValidationTest() override = default;
};

TEST_P(ExpirationDateValidationTest, ExpirationDateValidation) {
  auto exp_case = GetParam();
  ShowPrompt();
  EXPECT_EQ(exp_case.valid, controller_->InputExpirationIsValid(
                                ASCIIToUTF16(exp_case.input_month),
                                ASCIIToUTF16(exp_case.input_year)));
}

INSTANTIATE_TEST_SUITE_P(
    CardUnmaskPromptControllerImplGenericTest,
    ExpirationDateValidationTest,
    testing::Values(ExpirationDateTestCase{"01", "2040", true},
                    ExpirationDateTestCase{"1", "2040", true},
                    ExpirationDateTestCase{"1", "40", true},
                    ExpirationDateTestCase{"10", "40", true},
                    ExpirationDateTestCase{"01", "1940", false},
                    ExpirationDateTestCase{"13", "2040", false}));

class VirtualCardErrorTest
    : public CardUnmaskPromptControllerImplGenericTest,
      public testing::WithParamInterface<PaymentsRpcResult> {
 public:
  VirtualCardErrorTest() = default;

  VirtualCardErrorTest(const VirtualCardErrorTest&) = delete;
  VirtualCardErrorTest& operator=(const VirtualCardErrorTest&) = delete;

  ~VirtualCardErrorTest() override = default;
};

#if BUILDFLAG(IS_ANDROID)
TEST_P(VirtualCardErrorTest, VirtualCardFailureDismissesUnmaskPrompt) {
  ShowPromptAndSimulateResponse(/*enable_fido_auth=*/false,
                                /*should_unmask_virtual_card=*/true);
  base::HistogramTester histogram_tester;

  controller_->OnVerificationResult(GetParam());

  // Verify that the dialog is closed by checking the state.
  EXPECT_EQ(PaymentsRpcResult::kNone, controller_->GetVerificationResult());
  // Verify that prompt closing metrics are logged.
  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.VirtualCard.Events",
      AutofillMetrics::
          UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_NON_RETRIABLE_FAILURE,
      1);
  histogram_tester.ExpectTotalCount("Autofill.UnmaskPrompt.Duration", 1);
  histogram_tester.ExpectTotalCount("Autofill.UnmaskPrompt.Duration.Failure",
                                    1);
}

INSTANTIATE_TEST_SUITE_P(
    CardUnmaskPromptControllerImplGenericTest,
    VirtualCardErrorTest,
    testing::Values(PaymentsRpcResult::kVcnRetrievalPermanentFailure,
                    PaymentsRpcResult::kVcnRetrievalTryAgainFailure));
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace autofill
