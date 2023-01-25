// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/touch_to_fill_delegate_impl.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::NiceMock;
using testing::Pointee;
using testing::Ref;
using testing::Return;

namespace autofill {

namespace {

class MockAutofillDriver : public TestAutofillDriver {
 public:
  MockAutofillDriver() = default;
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;
  ~MockAutofillDriver() override = default;

  MOCK_METHOD(bool, CanShowAutofillUi, (), (const, override));
};

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
  MockAutofillClient(const MockAutofillClient&) = delete;
  MockAutofillClient& operator=(const MockAutofillClient&) = delete;
  ~MockAutofillClient() override = default;

  MOCK_METHOD(void,
              ScanCreditCard,
              (CreditCardScanCallback callback),
              (override));
  MOCK_METHOD(bool, IsTouchToFillCreditCardSupported, (), (override));
  MOCK_METHOD(void,
              ShowAutofillSettings,
              (bool show_credit_card_settings),
              (override));
  MOCK_METHOD(bool,
              ShowTouchToFillCreditCard,
              (base::WeakPtr<autofill::TouchToFillDelegate> delegate,
               base::span<const autofill::CreditCard* const> cards_to_suggest),
              (override));
  MOCK_METHOD(void, HideTouchToFillCreditCard, (), (override));
  MOCK_METHOD(void, HideAutofillPopup, (PopupHidingReason reason), (override));

  void ExpectDelegateWeakPtrFromShowInvalidatedOnHide() {
    EXPECT_CALL(*this, ShowTouchToFillCreditCard)
        .WillOnce([this](base::WeakPtr<autofill::TouchToFillDelegate> delegate,
                         base::span<const autofill::CreditCard* const>
                             cards_to_suggest) {
          captured_delegate_ = delegate;
          return true;
        });
    EXPECT_CALL(*this, HideTouchToFillCreditCard).WillOnce([this]() {
      EXPECT_FALSE(captured_delegate_);
    });
  }

 private:
  base::WeakPtr<autofill::TouchToFillDelegate> captured_delegate_;
};

class MockBrowserAutofillManager : public TestBrowserAutofillManager {
 public:
  MockBrowserAutofillManager(TestAutofillDriver* driver,
                             TestAutofillClient* client)
      : TestBrowserAutofillManager(driver, client) {}
  MockBrowserAutofillManager(const MockBrowserAutofillManager&) = delete;
  MockBrowserAutofillManager& operator=(const MockBrowserAutofillManager&) =
      delete;
  ~MockBrowserAutofillManager() override = default;

  MOCK_METHOD(PopupType,
              GetPopupType,
              (const FormData& form, const FormFieldData& field),
              (override));
  MOCK_METHOD(void,
              FillCreditCardFormImpl,
              (const FormData& form,
               const FormFieldData& field,
               const CreditCard& credit_card,
               const std::u16string& cvc),
              (override));
  MOCK_METHOD(void,
              FillOrPreviewCreditCardForm,
              (mojom::RendererFormDataAction action,
               const FormData& form,
               const FormFieldData& field,
               const CreditCard* credit_card));
  MOCK_METHOD(void,
              SetAutofillSuggestionMethod,
              (AutofillSuggestionMethod state),
              (override));
};

}  // namespace

class TouchToFillDelegateImplUnitTest : public testing::Test {
 protected:
  void SetUp() override {
    test::CreateTestCreditCardFormData(&form_, /*is_https=*/true,
                                       /*use_month_type=*/false);

    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    autofill_client_.GetPersonalDataManager()->SetPrefService(
        autofill_client_.GetPrefs());
    autofill_driver_ = std::make_unique<NiceMock<MockAutofillDriver>>();
    browser_autofill_manager_ =
        std::make_unique<NiceMock<MockBrowserAutofillManager>>(
            autofill_driver_.get(), &autofill_client_);

    auto touch_to_fill_delegate = std::make_unique<TouchToFillDelegateImpl>(
        browser_autofill_manager_.get());
    touch_to_fill_delegate_ = touch_to_fill_delegate.get();
    base::WeakPtr<TouchToFillDelegateImpl> touch_to_fill_delegate_weak =
        touch_to_fill_delegate->GetWeakPtr();
    browser_autofill_manager_->SetTouchToFillDelegateImplForTest(
        std::move(touch_to_fill_delegate));

    // Default setup for successful `TryToShowTouchToFill`.
    field_.is_focusable = true;
    autofill_client_.GetPersonalDataManager()->AddCreditCard(
        test::GetCreditCard());
    ON_CALL(*browser_autofill_manager_, GetPopupType(_, _))
        .WillByDefault(Return(PopupType::kCreditCards));
    ON_CALL(autofill_client_, IsTouchToFillCreditCardSupported)
        .WillByDefault(Return(true));
    ON_CALL(*autofill_driver_, CanShowAutofillUi).WillByDefault(Return(true));
    ON_CALL(autofill_client_, ShowTouchToFillCreditCard)
        .WillByDefault(Return(true));
    // Calling HideTouchToFillCreditCard in production code leads to that
    // OnDismissed gets triggered (HideTouchToFillCreditCard calls view->Hide()
    // on java side, which in its turn triggers onDismissed). Here we mock this
    // call.
    ON_CALL(autofill_client_, HideTouchToFillCreditCard)
        .WillByDefault([delegate = touch_to_fill_delegate_weak]() -> void {
          if (delegate) {
            delegate->OnDismissed(/*dismissed_by_user=*/false);
          }
        });
  }

  void TryToShowTouchToFill(bool expected_success) {
    EXPECT_CALL(autofill_client_,
                HideAutofillPopup(
                    PopupHidingReason::kOverlappingWithTouchToFillSurface))
        .Times(expected_success ? 1 : 0);
    EXPECT_EQ(expected_success,
              touch_to_fill_delegate_->TryToShowTouchToFill(form_, field_));
    EXPECT_EQ(expected_success,
              touch_to_fill_delegate_->IsShowingTouchToFill());
  }

  FormData form_;
  FormFieldData field_;

  base::test::TaskEnvironment task_environment_;
  test::AutofillEnvironment autofill_environment_;
  NiceMock<MockAutofillClient> autofill_client_;
  std::unique_ptr<NiceMock<MockAutofillDriver>> autofill_driver_;
  std::unique_ptr<MockBrowserAutofillManager> browser_autofill_manager_;
  raw_ptr<TouchToFillDelegateImpl> touch_to_fill_delegate_;
  base::HistogramTester histogram_tester_;
};

TEST_F(TouchToFillDelegateImplUnitTest, TryToShowTouchToFillSucceeds) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/true);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kShown, 1);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillFailsIfNotCreditCardField) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(*browser_autofill_manager_, GetPopupType(Ref(form_), Ref(field_)))
      .WillOnce(Return(PopupType::kAddresses));

  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillFailsIfNotSupported) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(autofill_client_, IsTouchToFillCreditCardSupported)
      .WillOnce(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillFailsIfFormIsNotSecure) {
  // Simulate non-secure form.
  test::CreateTestCreditCardFormData(&form_, /*is_https=*/false,
                                     /*use_month_type=*/false);

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kFormOrClientNotSecure, 1);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillFailsIfClientIsNotSecure) {
  // Simulate non-secure client.
  autofill_client_.set_form_origin(GURL("http://example.com"));

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kFormOrClientNotSecure, 1);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillFailsIfAlreadyShown) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(
      autofill_client_,
      HideAutofillPopup(PopupHidingReason::kOverlappingWithTouchToFillSurface))
      .Times(0);
  EXPECT_FALSE(touch_to_fill_delegate_->TryToShowTouchToFill(form_, field_));
  EXPECT_TRUE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_F(TouchToFillDelegateImplUnitTest, TryToShowTouchToFillFailsIfWasShown) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->HideTouchToFill();

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectBucketCount(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kShownBefore, 1);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillFailsIfFieldIsNotFocusable) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  field_.is_focusable = false;

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kFieldNotEmptyOrNotFocusable, 1);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillFailsIfFieldHasValue) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  field_.value = u"Initial value";

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kFieldNotEmptyOrNotFocusable, 1);

  // But should ignore formatting characters.
  field_.value = u"____-____-____-____";

  TryToShowTouchToFill(/*expected_success=*/true);
  histogram_tester_.ExpectBucketCount(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kShown, 1);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillFailsIfNoCardsOnFile) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kNoValidCards, 1);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillFailsIfCardIsIncomplete) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard cc_no_number = test::GetCreditCard();
  cc_no_number.SetNumber(u"");
  autofill_client_.GetPersonalDataManager()->AddCreditCard(cc_no_number);

  TryToShowTouchToFill(/*expected_success=*/false);

  CreditCard cc_no_exp_date = test::GetCreditCard();
  cc_no_exp_date.SetExpirationMonth(0);
  cc_no_exp_date.SetExpirationYear(0);
  autofill_client_.GetPersonalDataManager()->AddCreditCard(cc_no_exp_date);

  TryToShowTouchToFill(/*expected_success=*/false);

  CreditCard cc_no_name = test::GetCreditCard();
  cc_no_name.SetRawInfo(CREDIT_CARD_NAME_FULL, u"");
  autofill_client_.GetPersonalDataManager()->AddCreditCard(cc_no_name);

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kNoValidCards, 3);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillFailsIfTheOnlyCardIsExpired) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  autofill_client_.GetPersonalDataManager()->AddCreditCard(
      test::GetExpiredCreditCard());

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kNoValidCards, 1);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillFailsIfCardNumberIsInvalid) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard cc_invalid_number = test::GetCreditCard();
  cc_invalid_number.SetNumber(u"invalid number");
  autofill_client_.GetPersonalDataManager()->AddCreditCard(cc_invalid_number);

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kNoValidCards, 1);

  // But succeeds for existing masked server card with incomplete number.
  autofill_client_.GetPersonalDataManager()->AddCreditCard(
      test::GetMaskedServerCard());

  TryToShowTouchToFill(/*expected_success=*/true);
  histogram_tester_.ExpectBucketCount(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kShown, 1);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillFailsIfCanNotShowUi) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(*autofill_driver_, CanShowAutofillUi).WillOnce(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillCreditCardTriggerOutcome::kCannotShowAutofillUi, 1);
}

TEST_F(TouchToFillDelegateImplUnitTest, TryToShowTouchToFillFailsIfShowFails) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(autofill_client_, ShowTouchToFillCreditCard)
      .WillOnce(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillSucceedsIfAtLestOneCardIsValid) {
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  CreditCard expired_card = test::GetExpiredCreditCard();
  autofill_client_.GetPersonalDataManager()->AddCreditCard(credit_card);
  autofill_client_.GetPersonalDataManager()->AddCreditCard(expired_card);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(autofill_client_, ShowTouchToFillCreditCard)
      .WillOnce(Return(true));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateImplUnitTest, TryToShowTouchToFillShowsExpiredCards) {
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  CreditCard expired_card = test::GetExpiredCreditCard();
  autofill_client_.GetPersonalDataManager()->AddCreditCard(credit_card);
  autofill_client_.GetPersonalDataManager()->AddCreditCard(expired_card);
  std::vector<autofill::CreditCard*> credit_cards =
      autofill_client_.GetPersonalDataManager()->GetCreditCardsToSuggest();
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(autofill_client_,
              ShowTouchToFillCreditCard(_, ElementsAreArray(credit_cards)));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillDoesNotShowDisusedExpiredCards) {
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  credit_card.set_use_date(AutofillClock::Now());
  CreditCard disused_expired_card = test::GetExpiredCreditCard();
  const base::Time last_used =
      AutofillClock::Now() - kDisusedDataModelTimeDelta * 2;
  disused_expired_card.set_use_date(last_used);
  autofill_client_.GetPersonalDataManager()->AddCreditCard(credit_card);
  autofill_client_.GetPersonalDataManager()->AddCreditCard(
      disused_expired_card);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(autofill_client_,
              ShowTouchToFillCreditCard(_, ElementsAre(Pointee(credit_card))));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateImplUnitTest, HideTouchToFillDoesNothingIfNotShown) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  EXPECT_CALL(autofill_client_, HideTouchToFillCreditCard).Times(0);
  touch_to_fill_delegate_->HideTouchToFill();
  EXPECT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_F(TouchToFillDelegateImplUnitTest, HideTouchToFillHidesIfShown) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(autofill_client_, HideTouchToFillCreditCard).Times(1);
  touch_to_fill_delegate_->HideTouchToFill();
  EXPECT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_F(TouchToFillDelegateImplUnitTest, ResetHidesTouchToFillIfShown) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(autofill_client_, HideTouchToFillCreditCard).Times(1);
  touch_to_fill_delegate_->Reset();
  EXPECT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_F(TouchToFillDelegateImplUnitTest, ResetAllowsShowingTouchToFillAgain) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->HideTouchToFill();
  TryToShowTouchToFill(/*expected_success=*/false);

  touch_to_fill_delegate_->Reset();
  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateImplUnitTest, SafelyHideTouchToFillInDtor) {
  autofill_client_.ExpectDelegateWeakPtrFromShowInvalidatedOnHide();
  TryToShowTouchToFill(/*expected_success=*/true);

  browser_autofill_manager_.reset();
}

TEST_F(TouchToFillDelegateImplUnitTest,
       OnDismissSetsTouchToFillToNotShowingState) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->OnDismissed(false);

  EXPECT_EQ(touch_to_fill_delegate_->IsShowingTouchToFill(), false);
}

TEST_F(TouchToFillDelegateImplUnitTest, PassTheCreditCardsToTheClient) {
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard credit_card1 = autofill::test::GetCreditCard();
  CreditCard credit_card2 = autofill::test::GetCreditCard2();
  autofill_client_.GetPersonalDataManager()->AddCreditCard(credit_card1);
  autofill_client_.GetPersonalDataManager()->AddCreditCard(credit_card2);
  std::vector<autofill::CreditCard*> credit_cards =
      autofill_client_.GetPersonalDataManager()->GetCreditCardsToSuggest();

  EXPECT_CALL(autofill_client_,
              ShowTouchToFillCreditCard(_, ElementsAreArray(credit_cards)));

  TryToShowTouchToFill(/*expected_success=*/true);

  browser_autofill_manager_.reset();
}

TEST_F(TouchToFillDelegateImplUnitTest, ScanCreditCardIsCalled) {
  TryToShowTouchToFill(/*expected_success=*/true);
  EXPECT_CALL(autofill_client_, ScanCreditCard);
  touch_to_fill_delegate_->ScanCreditCard();

  CreditCard credit_card = autofill::test::GetCreditCard();
  EXPECT_CALL(*browser_autofill_manager_, FillCreditCardFormImpl);
  touch_to_fill_delegate_->OnCreditCardScanned(credit_card);
  EXPECT_EQ(touch_to_fill_delegate_->IsShowingTouchToFill(), false);
}

TEST_F(TouchToFillDelegateImplUnitTest, ShowCreditCardSettingsIsCalled) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(autofill_client_, ShowAutofillSettings(testing::Eq(true)));
  touch_to_fill_delegate_->ShowCreditCardSettings();

  ASSERT_EQ(touch_to_fill_delegate_->IsShowingTouchToFill(), false);
}

TEST_F(TouchToFillDelegateImplUnitTest, CardSelectionClosesTheSheet) {
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  autofill_client_.GetPersonalDataManager()->AddCreditCard(credit_card);

  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(autofill_client_, HideTouchToFillCreditCard).Times(1);
  touch_to_fill_delegate_->SuggestionSelected(credit_card.server_id());
}

TEST_F(TouchToFillDelegateImplUnitTest, CardSelectionFillsCardForm) {
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  autofill_client_.GetPersonalDataManager()->AddCreditCard(credit_card);

  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(*browser_autofill_manager_, FillOrPreviewCreditCardForm);
  EXPECT_CALL(*browser_autofill_manager_, SetAutofillSuggestionMethod);
  touch_to_fill_delegate_->SuggestionSelected(credit_card.server_id());
}

TEST_F(TouchToFillDelegateImplUnitTest, SubmissionMetricsAreTracked) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->OnDismissed(/*dismissed_by_user=*/true);

  // Simulate that the form was autofilled by other means
  FormStructure submitted_form(form_);
  for (const std::unique_ptr<AutofillField>& field : submitted_form) {
    field->is_autofilled = true;
  }

  touch_to_fill_delegate_->LogMetricsAfterSubmission(submitted_form);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.TouchToFill.CreditCard.AutofillUsedAfterTouchToFillDismissal",
      true, 1);
}

}  // namespace autofill
