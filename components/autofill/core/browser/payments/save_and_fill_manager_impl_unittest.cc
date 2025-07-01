// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/save_and_fill_manager_impl.h"

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

namespace {

class TestPaymentsAutofillClientMock : public TestPaymentsAutofillClient {
 public:
  explicit TestPaymentsAutofillClientMock(autofill::AutofillClient* client)
      : TestPaymentsAutofillClient(client) {}

  ~TestPaymentsAutofillClientMock() override = default;

  MOCK_METHOD(
      void,
      ShowCreditCardLocalSaveAndFillDialog,
      (base::OnceCallback<void(CardSaveAndFillDialogUserDecision,
                               const UserProvidedCardSaveAndFillDetails&)>),
      (override));
};

}  // namespace

class SaveAndFillManagerImplTest : public testing::Test {
 public:
  void SetUp() override {
    autofill_client_ = std::make_unique<autofill::TestAutofillClient>();
    payments_autofill_client_ =
        std::make_unique<TestPaymentsAutofillClientMock>(
            autofill_client_.get());
    save_and_fill_manager_impl_ = std::make_unique<SaveAndFillManagerImpl>(
        payments_autofill_client_.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<autofill::TestAutofillClient> autofill_client_;
  std::unique_ptr<TestPaymentsAutofillClientMock> payments_autofill_client_;
  std::unique_ptr<SaveAndFillManagerImpl> save_and_fill_manager_impl_;
};

TEST_F(SaveAndFillManagerImplTest, OfferLocalSaveAndFill_ShowsLocalDialog) {
  EXPECT_CALL(
      *payments_autofill_client_,
      ShowCreditCardLocalSaveAndFillDialog(
          testing::A<PaymentsAutofillClient::CardSaveAndFillDialogCallback>()));

  save_and_fill_manager_impl_->OfferLocalSaveAndFill();
}

}  // namespace autofill::payments
