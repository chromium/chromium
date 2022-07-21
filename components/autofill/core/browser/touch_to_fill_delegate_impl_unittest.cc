// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/touch_to_fill_delegate_impl.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Ref;
using testing::Return;

namespace autofill {

namespace {

// A constant value to use as a suggestions query ID.
const int kQueryId = 1;

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

  MOCK_METHOD(bool, IsTouchToFillCreditCardSupported, (), (override));
  MOCK_METHOD(bool,
              ShowTouchToFillCreditCard,
              (base::WeakPtr<autofill::TouchToFillDelegate>),
              (override));
  MOCK_METHOD(void, HideTouchToFillCreditCard, (), (override));
  MOCK_METHOD(void, HideAutofillPopup, (PopupHidingReason reason), (override));
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
};

}  // namespace

class TouchToFillDelegateImplUnitTest : public testing::Test {
 protected:
  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    autofill_client_.GetPersonalDataManager()->SetPrefService(
        autofill_client_.GetPrefs());
    autofill_driver_ = std::make_unique<NiceMock<MockAutofillDriver>>();
    browser_autofill_manager_ =
        std::make_unique<NiceMock<MockBrowserAutofillManager>>(
            autofill_driver_.get(), &autofill_client_);
    touch_to_fill_delegate_ = std::make_unique<TouchToFillDelegateImpl>(
        browser_autofill_manager_.get());

    // Default setup for successful |TryToShowTouchToFill|.
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
  }

  void TearDown() override {
    browser_autofill_manager_.reset();
    touch_to_fill_delegate_.reset();
    autofill_driver_.reset();
  }

  void TryToShowTouchToFill(bool expected_success) {
    EXPECT_CALL(autofill_client_,
                HideAutofillPopup(
                    PopupHidingReason::kOverlappingWithTouchToFillSurface))
        .Times(expected_success ? 1 : 0);
    EXPECT_EQ(expected_success, touch_to_fill_delegate_->TryToShowTouchToFill(
                                    kQueryId, form_, field_));
    EXPECT_EQ(expected_success,
              touch_to_fill_delegate_->IsShowingTouchToFill());
  }

  FormData form_;
  FormFieldData field_;

  base::test::TaskEnvironment task_environment_;
  NiceMock<MockAutofillClient> autofill_client_;
  std::unique_ptr<NiceMock<MockAutofillDriver>> autofill_driver_;
  std::unique_ptr<TouchToFillDelegateImpl> touch_to_fill_delegate_;
  std::unique_ptr<MockBrowserAutofillManager> browser_autofill_manager_;
};

TEST_F(TouchToFillDelegateImplUnitTest, TryToShowTouchToFillSucceeds) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/true);
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
       TryToShowTouchToFillFailsIfAlreadyShown) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(
      autofill_client_,
      HideAutofillPopup(PopupHidingReason::kOverlappingWithTouchToFillSurface))
      .Times(0);
  EXPECT_FALSE(
      touch_to_fill_delegate_->TryToShowTouchToFill(kQueryId, form_, field_));
  EXPECT_TRUE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_F(TouchToFillDelegateImplUnitTest, TryToShowTouchToFillFailsIfWasShown) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->HideTouchToFill();

  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillFailsIfFieldIsNotFocusable) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  field_.is_focusable = false;

  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillFailsIfFieldHasValue) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  field_.value = u"Initial value";

  TryToShowTouchToFill(/*expected_success=*/false);

  // But should ignore formatting characters.
  field_.value = u"____-____-____-____";

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillFailsIfNoCardsOnFile) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();

  TryToShowTouchToFill(/*expected_success=*/false);
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
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillFailsIfCardIsExpired) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  autofill_client_.GetPersonalDataManager()->AddCreditCard(
      test::GetExpiredCreditCard());

  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillFailsIfCardNumberIsInvalid) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()->ClearCreditCards();
  CreditCard cc_invalid_number = test::GetCreditCard();
  cc_invalid_number.SetNumber(u"invalid number");
  autofill_client_.GetPersonalDataManager()->AddCreditCard(cc_invalid_number);

  TryToShowTouchToFill(/*expected_success=*/false);

  // But succeeds for existing masked server card with incomplete number.
  autofill_client_.GetPersonalDataManager()->AddCreditCard(
      test::GetMaskedServerCard());

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateImplUnitTest,
       TryToShowTouchToFillFailsIfCanNotShowUi) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(*autofill_driver_, CanShowAutofillUi).WillOnce(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_F(TouchToFillDelegateImplUnitTest, TryToShowTouchToFillFailsIfShowFails) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(autofill_client_, ShowTouchToFillCreditCard)
      .WillOnce(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
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

}  // namespace autofill
