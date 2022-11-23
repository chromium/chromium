// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/fast_checkout_delegate_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/fast_checkout_delegate.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using base::BucketsAre;
using testing::_;
using testing::NiceMock;
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

  MOCK_METHOD(bool, IsFastCheckoutSupported, (), (override));
  MOCK_METHOD(bool,
              IsFastCheckoutTriggerForm,
              (const FormData&, const FormFieldData&),
              (override));
  MOCK_METHOD(bool,
              ShowFastCheckout,
              (base::WeakPtr<FastCheckoutDelegate>),
              (override));
  MOCK_METHOD(void, HideFastCheckout, (), (override));
  MOCK_METHOD(void, HideAutofillPopup, (PopupHidingReason reason), (override));

  void ExpectDelegateWeakPtrFromShowInvalidatedOnHide() {
    EXPECT_CALL(*this, ShowFastCheckout)
        .WillOnce([this](base::WeakPtr<FastCheckoutDelegate> delegate) {
          captured_delegate_ = delegate;
          return true;
        });
    EXPECT_CALL(*this, HideFastCheckout).WillOnce([this]() {
      EXPECT_FALSE(captured_delegate_);
    });
  }

 private:
  base::WeakPtr<FastCheckoutDelegate> captured_delegate_;
};

}  // namespace

class FastCheckoutDelegateImplTest : public testing::Test {
 protected:
  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    autofill_driver_ = std::make_unique<NiceMock<MockAutofillDriver>>();
    browser_autofill_manager_ = std::make_unique<TestBrowserAutofillManager>(
        autofill_driver_.get(), &autofill_client_);

    auto fast_checkout_delegate = std::make_unique<FastCheckoutDelegateImpl>(
        browser_autofill_manager_.get());
    fast_checkout_delegate_ = fast_checkout_delegate.get();

    browser_autofill_manager_->SetFastCheckoutDelegateForTest(
        std::move(fast_checkout_delegate));

    field_.is_focusable = true;
    ON_CALL(autofill_client_, IsFastCheckoutSupported)
        .WillByDefault(Return(true));
    ON_CALL(autofill_client_, IsFastCheckoutTriggerForm)
        .WillByDefault(Return(true));
    ON_CALL(autofill_client_, ShowFastCheckout).WillByDefault(Return(true));
    ON_CALL(*autofill_driver_, CanShowAutofillUi).WillByDefault(Return(true));
  }

  void TryToShowFastCheckout(bool expected_success) {
    EXPECT_CALL(autofill_client_,
                HideAutofillPopup(
                    PopupHidingReason::kOverlappingWithFastCheckoutSurface))
        .Times(expected_success ? 1 : 0);
    EXPECT_EQ(expected_success,
              fast_checkout_delegate_->TryToShowFastCheckout(form_, field_));
    EXPECT_EQ(expected_success,
              fast_checkout_delegate_->IsShowingFastCheckoutUI());
  }

  FormData form_;
  FormFieldData field_;

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  NiceMock<MockAutofillClient> autofill_client_;
  std::unique_ptr<NiceMock<MockAutofillDriver>> autofill_driver_;
  std::unique_ptr<TestBrowserAutofillManager> browser_autofill_manager_;
  raw_ptr<FastCheckoutDelegateImpl> fast_checkout_delegate_;
};

TEST_F(FastCheckoutDelegateImplTest, TryToShowFastCheckoutSucceeds) {
  ASSERT_FALSE(fast_checkout_delegate_->IsShowingFastCheckoutUI());
  TryToShowFastCheckout(/*expected_success=*/true);

  histogram_tester_.ExpectUniqueSample(kUmaKeyFastCheckoutTriggerOutcome,
                                       FastCheckoutTriggerOutcome::kSuccess,
                                       1u);
}

TEST_F(FastCheckoutDelegateImplTest, TryToShowFastCheckoutFailsIfNotSupported) {
  ASSERT_FALSE(fast_checkout_delegate_->IsShowingFastCheckoutUI());
  EXPECT_CALL(autofill_client_, IsFastCheckoutSupported)
      .WillOnce(Return(false));
  TryToShowFastCheckout(/*expected_success=*/false);

  // Events are only logged if Fast Checkout is supported and there is a script.
  histogram_tester_.ExpectTotalCount(kUmaKeyFastCheckoutTriggerOutcome, 0u);
}

TEST_F(FastCheckoutDelegateImplTest,
       TryToShowFastCheckoutFailsIfFormNotTriggering) {
  ASSERT_FALSE(fast_checkout_delegate_->IsShowingFastCheckoutUI());
  EXPECT_CALL(autofill_client_, IsFastCheckoutTriggerForm)
      .WillOnce(Return(false));
  TryToShowFastCheckout(/*expected_success=*/false);

  // Events are only logged if Fast Checkout is supported and there is a script.
  histogram_tester_.ExpectTotalCount(kUmaKeyFastCheckoutTriggerOutcome, 0u);
}

TEST_F(FastCheckoutDelegateImplTest,
       TryToShowFastCheckoutFailsIfAlreadyShowing) {
  TryToShowFastCheckout(/*expected_success=*/true);

  EXPECT_CALL(
      autofill_client_,
      HideAutofillPopup(PopupHidingReason::kOverlappingWithFastCheckoutSurface))
      .Times(0);
  EXPECT_FALSE(fast_checkout_delegate_->TryToShowFastCheckout(form_, field_));
  EXPECT_TRUE(fast_checkout_delegate_->IsShowingFastCheckoutUI());

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kUmaKeyFastCheckoutTriggerOutcome),
      BucketsAre(Bucket(FastCheckoutTriggerOutcome::kSuccess, 1u),
                 Bucket(FastCheckoutTriggerOutcome::kFailureShownBefore, 1u)));
}

TEST_F(FastCheckoutDelegateImplTest, TryToShowFastCheckoutFailsIfWasShown) {
  ASSERT_FALSE(fast_checkout_delegate_->IsShowingFastCheckoutUI());
  TryToShowFastCheckout(/*expected_success=*/true);
  // User accepts/dismisses the bottom sheet.
  fast_checkout_delegate_->OnFastCheckoutUIHidden();
  TryToShowFastCheckout(/*expected_success=*/false);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kUmaKeyFastCheckoutTriggerOutcome),
      BucketsAre(Bucket(FastCheckoutTriggerOutcome::kSuccess, 1u),
                 Bucket(FastCheckoutTriggerOutcome::kFailureShownBefore, 1u)));
}

TEST_F(FastCheckoutDelegateImplTest,
       TryToShowFastCheckoutFailsIfFieldNotFocusable) {
  ASSERT_FALSE(fast_checkout_delegate_->IsShowingFastCheckoutUI());
  field_.is_focusable = false;
  TryToShowFastCheckout(/*expected_success=*/false);

  histogram_tester_.ExpectUniqueSample(
      kUmaKeyFastCheckoutTriggerOutcome,
      FastCheckoutTriggerOutcome::kFailureFieldNotFocusable, 1u);
}

TEST_F(FastCheckoutDelegateImplTest,
       TryToShowFastCheckoutFailsIfFieldHasValue) {
  ASSERT_FALSE(fast_checkout_delegate_->IsShowingFastCheckoutUI());
  field_.value = u"Initial value";
  TryToShowFastCheckout(/*expected_success=*/false);

  histogram_tester_.ExpectUniqueSample(
      kUmaKeyFastCheckoutTriggerOutcome,
      FastCheckoutTriggerOutcome::kFailureFieldNotEmpty, 1u);
}

TEST_F(FastCheckoutDelegateImplTest, TryToShowFastCheckoutFailsIfCanNotShowUi) {
  ASSERT_FALSE(fast_checkout_delegate_->IsShowingFastCheckoutUI());
  EXPECT_CALL(*autofill_driver_, CanShowAutofillUi).WillOnce(Return(false));
  TryToShowFastCheckout(/*expected_success=*/false);

  histogram_tester_.ExpectUniqueSample(
      kUmaKeyFastCheckoutTriggerOutcome,
      FastCheckoutTriggerOutcome::kFailureCannotShowAutofillUi, 1u);
}

TEST_F(FastCheckoutDelegateImplTest, TryToShowFastCheckoutFailsIfShowFails) {
  ASSERT_FALSE(fast_checkout_delegate_->IsShowingFastCheckoutUI());
  EXPECT_CALL(autofill_client_, ShowFastCheckout).WillOnce(Return(false));
  TryToShowFastCheckout(/*expected_success=*/false);
}

TEST_F(FastCheckoutDelegateImplTest, HideFastCheckoutDoesNothingIfNotShown) {
  ASSERT_FALSE(fast_checkout_delegate_->IsShowingFastCheckoutUI());
  EXPECT_CALL(autofill_client_, HideFastCheckout).Times(0);
  fast_checkout_delegate_->HideFastCheckoutUI();
  EXPECT_FALSE(fast_checkout_delegate_->IsShowingFastCheckoutUI());
}

TEST_F(FastCheckoutDelegateImplTest, HideFastCheckoutHidesIfShown) {
  TryToShowFastCheckout(/*expected_success=*/true);
  EXPECT_CALL(autofill_client_, HideFastCheckout).Times(1);
  fast_checkout_delegate_->HideFastCheckoutUI();
  EXPECT_FALSE(fast_checkout_delegate_->IsShowingFastCheckoutUI());
}

TEST_F(FastCheckoutDelegateImplTest, GetDriver) {
  EXPECT_EQ(autofill_driver_.get(), fast_checkout_delegate_->GetDriver());
}

TEST_F(FastCheckoutDelegateImplTest, ResetFastCheckoutIfShown) {
  TryToShowFastCheckout(/*expected_success=*/true);
  EXPECT_CALL(autofill_client_, HideFastCheckout).Times(1);
  fast_checkout_delegate_->Reset();
  EXPECT_FALSE(fast_checkout_delegate_->IsShowingFastCheckoutUI());
}

TEST_F(FastCheckoutDelegateImplTest, ResetAllowsShowingFastCheckoutAgain) {
  TryToShowFastCheckout(/*expected_success=*/true);

  fast_checkout_delegate_->HideFastCheckoutUI();
  TryToShowFastCheckout(/*expected_success=*/false);

  fast_checkout_delegate_->Reset();
  TryToShowFastCheckout(/*expected_success=*/true);
}

TEST_F(FastCheckoutDelegateImplTest, SafelyHideFastCheckoutInDtor) {
  autofill_client_.ExpectDelegateWeakPtrFromShowInvalidatedOnHide();
  TryToShowFastCheckout(/*expected_success=*/true);
  browser_autofill_manager_.reset();
}

}  // namespace autofill
