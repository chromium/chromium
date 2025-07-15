// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/save_and_fill_manager_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

namespace {

using base::ASCIIToUTF16;

using CardSaveAndFillDialogUserDecision =
    PaymentsAutofillClient::CardSaveAndFillDialogUserDecision;
using UserProvidedCardSaveAndFillDetails =
    PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails;

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
    autofill_client_->SetPrefs(test::PrefServiceForTesting());
    autofill_client_->GetPersonalDataManager().SetPrefService(
        autofill_client_->GetPrefs());
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

UserProvidedCardSaveAndFillDetails CreateUserProvidedCardDetails(
    std::u16string card_number,
    std::u16string cardholder_name,
    std::u16string expiration_date_month,
    std::u16string expiration_date_year,
    std::optional<std::u16string> security_code) {
  UserProvidedCardSaveAndFillDetails user_provided_card_details;
  user_provided_card_details.card_number = std::move(card_number);
  user_provided_card_details.cardholder_name = std::move(cardholder_name);
  user_provided_card_details.expiration_date_month =
      std::move(expiration_date_month);
  user_provided_card_details.expiration_date_year =
      std::move(expiration_date_year);
  user_provided_card_details.security_code = std::move(security_code);
  return user_provided_card_details;
}

TEST_F(SaveAndFillManagerImplTest, OfferLocalSaveAndFill_ShowsLocalDialog) {
  EXPECT_CALL(
      *payments_autofill_client_,
      ShowCreditCardLocalSaveAndFillDialog(
          testing::A<PaymentsAutofillClient::CardSaveAndFillDialogCallback>()));

  save_and_fill_manager_impl_->OfferLocalSaveAndFill();
}

TEST_F(SaveAndFillManagerImplTest, OnUserDidDecideOnLocalSave_Accepted) {
  save_and_fill_manager_impl_->OnUserDidDecideOnLocalSave(
      CardSaveAndFillDialogUserDecision::kAccepted,
      CreateUserProvidedCardDetails(
          /*card_number=*/u"4444333322221111", /*cardholder_name=*/u"John Doe",
          /*expiration_date_month=*/ASCIIToUTF16(test::NextMonth()),
          /*expiration_date_year=*/ASCIIToUTF16(test::NextYear()),
          /*security_code=*/u"123"));

  EXPECT_THAT(payments_autofill_client_->GetPaymentsDataManager()
                  .GetCreditCards()
                  .size(),
              1u);

  const CreditCard* saved_card =
      payments_autofill_client_->GetPaymentsDataManager()
          .GetLocalCreditCards()[0];

  EXPECT_EQ(u"4444333322221111", saved_card->GetRawInfo(CREDIT_CARD_NUMBER));
  EXPECT_EQ(u"John Doe", saved_card->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16(test::NextMonth()),
            saved_card->GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(ASCIIToUTF16(test::NextYear()),
            saved_card->GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

TEST_F(SaveAndFillManagerImplTest, OnUserDidDecideOnLocalSave_Declined) {
  save_and_fill_manager_impl_->OnUserDidDecideOnLocalSave(
      CardSaveAndFillDialogUserDecision::kDeclined,
      UserProvidedCardSaveAndFillDetails());

  EXPECT_TRUE(payments_autofill_client_->GetPaymentsDataManager()
                  .GetCreditCards()
                  .empty());
}

#if !BUILDFLAG(IS_IOS)
TEST_F(SaveAndFillManagerImplTest, LocallySaveCreditCard_WithCvc_PrefOn) {
  prefs::SetPaymentCvcStorage(autofill_client_->GetPrefs(), true);

  save_and_fill_manager_impl_->OnUserDidDecideOnLocalSave(
      CardSaveAndFillDialogUserDecision::kAccepted,
      CreateUserProvidedCardDetails(
          /*card_number=*/u"4444333322221111", /*cardholder_name=*/u"John Doe",
          /*expiration_date_month=*/ASCIIToUTF16(test::NextMonth()),
          /*expiration_date_year=*/ASCIIToUTF16(test::NextYear()),
          /*security_code=*/u"123"));

  EXPECT_THAT(payments_autofill_client_->GetPaymentsDataManager()
                  .GetCreditCards()
                  .size(),
              1u);
  EXPECT_THAT(payments_autofill_client_->GetPaymentsDataManager()
                  .GetLocalCreditCards()
                  .front()
                  ->cvc(),
              u"123");
}

TEST_F(SaveAndFillManagerImplTest, LocallySaveCreditCard_WithCvc_PrefOff) {
  prefs::SetPaymentCvcStorage(autofill_client_->GetPrefs(), false);

  save_and_fill_manager_impl_->OnUserDidDecideOnLocalSave(
      CardSaveAndFillDialogUserDecision::kAccepted,
      CreateUserProvidedCardDetails(
          /*card_number=*/u"4444333322221111", /*cardholder_name=*/u"John Doe",
          /*expiration_date_month=*/ASCIIToUTF16(test::NextMonth()),
          /*expiration_date_year=*/ASCIIToUTF16(test::NextYear()),
          /*security_code=*/u"123"));

  EXPECT_THAT(payments_autofill_client_->GetPaymentsDataManager()
                  .GetCreditCards()
                  .size(),
              1u);
  EXPECT_THAT(payments_autofill_client_->GetPaymentsDataManager()
                  .GetLocalCreditCards()
                  .front()
                  ->cvc(),
              u"");
}
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace autofill::payments
