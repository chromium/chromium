// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/save_and_fill_manager_impl.h"

#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/test/mock_multiple_request_payments_network_interface.h"
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

constexpr char kLegalMessageLines[] =
    "{"
    "  \"line\" : [ {"
    "     \"template\": \"The legal documents are: {0} and {1}.\","
    "     \"template_parameter\" : [ {"
    "        \"display_text\" : \"Terms of Service\","
    "        \"url\": \"http://www.example.com/tos\""
    "     }, {"
    "        \"display_text\" : \"Privacy Policy\","
    "        \"url\": \"http://www.example.com/pp\""
    "     } ]"
    "  } ]"
    "}";

constexpr char kInvalidLegalMessageLines[] =
    "{"
    "  \"line\" : [ {"
    "     \"template\": \"Panda {0}.\","
    "     \"template_parameter\": [ {"
    "        \"display_text\": \"bear\""
    "     } ]"
    "  } ]"
    "}";

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

  MOCK_METHOD(void,
              ShowCreditCardUploadSaveAndFillDialog,
              (const LegalMessageLines&, CardSaveAndFillDialogCallback),
              (override));

  MOCK_METHOD(void,
              LoadRiskData,
              (base::OnceCallback<void(const std::string&)>),
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

    auto payments_autofill_client =
        std::make_unique<TestPaymentsAutofillClientMock>(
            autofill_client_.get());
    payments_autofill_client_ = payments_autofill_client.get();
    autofill_client_->set_payments_autofill_client(
        std::move(payments_autofill_client));

    auto mock_network_interface =
        std::make_unique<MockMultipleRequestPaymentsNetworkInterface>(
            autofill_client_->GetURLLoaderFactory(),
            *autofill_client_->GetIdentityManager());
    mock_network_interface_ = mock_network_interface.get();
    payments_autofill_client_->set_multiple_request_payments_network_interface(
        std::move(mock_network_interface));

    save_and_fill_manager_impl_ =
        std::make_unique<SaveAndFillManagerImpl>(autofill_client_.get());
  }

  void SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult result,
      bool create_valid_legal_message) {
    ON_CALL(*mock_network_interface_,
            GetDetailsForCreateCard(testing::_, testing::_))
        .WillByDefault([result, create_valid_legal_message](
                           const auto& /*request_details*/,
                           base::OnceCallback<void(
                               PaymentsAutofillClient::PaymentsRpcResult,
                               const std::u16string&,
                               std::unique_ptr<base::Value::Dict>,
                               std::vector<std::pair<int, int>>)> callback) {
          std::move(callback).Run(
              result, u"context_token",
              create_valid_legal_message
                  ? std::make_unique<base::Value::Dict>(
                        base::JSONReader::ReadDict(kLegalMessageLines).value())
                  : std::make_unique<base::Value::Dict>(
                        base::JSONReader::ReadDict(kInvalidLegalMessageLines)
                            .value()),
              {});
          return RequestId("11223344");
        });
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestAutofillClient> autofill_client_;
  raw_ptr<TestPaymentsAutofillClientMock> payments_autofill_client_;
  std::unique_ptr<SaveAndFillManagerImpl> save_and_fill_manager_impl_;
  raw_ptr<MockMultipleRequestPaymentsNetworkInterface> mock_network_interface_;
  base::MockCallback<SaveAndFillManagerImpl::FillCardCallback>
      fill_card_callback_;
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

  save_and_fill_manager_impl_->OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback_.Get());
}

TEST_F(SaveAndFillManagerImplTest, OnUserDidDecideOnLocalSave_Accepted) {
  EXPECT_CALL(
      *payments_autofill_client_,
      ShowCreditCardLocalSaveAndFillDialog(
          testing::A<PaymentsAutofillClient::CardSaveAndFillDialogCallback>()));

  CreditCard card_to_fill;
  EXPECT_CALL(fill_card_callback_, Run(testing::A<const CreditCard&>()))
      .WillOnce(testing::SaveArg<0>(&card_to_fill));

  save_and_fill_manager_impl_->OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback_.Get());
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

  EXPECT_EQ(u"4444333322221111", card_to_fill.GetRawInfo(CREDIT_CARD_NUMBER));
  EXPECT_EQ(u"John Doe", card_to_fill.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16(test::NextMonth()),
            card_to_fill.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(ASCIIToUTF16(test::NextYear()),
            card_to_fill.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

TEST_F(SaveAndFillManagerImplTest, OnUserDidDecideOnLocalSave_Declined) {
  EXPECT_CALL(
      *payments_autofill_client_,
      ShowCreditCardLocalSaveAndFillDialog(
          testing::A<PaymentsAutofillClient::CardSaveAndFillDialogCallback>()));

  save_and_fill_manager_impl_->OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback_.Get());
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

  EXPECT_CALL(
      *payments_autofill_client_,
      ShowCreditCardLocalSaveAndFillDialog(
          testing::A<PaymentsAutofillClient::CardSaveAndFillDialogCallback>()));

  CreditCard card_to_fill;
  EXPECT_CALL(fill_card_callback_, Run(testing::A<const CreditCard&>()))
      .WillOnce(testing::SaveArg<0>(&card_to_fill));

  save_and_fill_manager_impl_->OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback_.Get());
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
  EXPECT_THAT(card_to_fill.cvc(), u"123");
}

TEST_F(SaveAndFillManagerImplTest, LocallySaveCreditCard_WithCvc_PrefOff) {
  prefs::SetPaymentCvcStorage(autofill_client_->GetPrefs(), false);

  EXPECT_CALL(
      *payments_autofill_client_,
      ShowCreditCardLocalSaveAndFillDialog(
          testing::A<PaymentsAutofillClient::CardSaveAndFillDialogCallback>()));

  CreditCard card_to_fill;
  EXPECT_CALL(fill_card_callback_, Run(testing::A<const CreditCard&>()))
      .WillOnce(testing::SaveArg<0>(&card_to_fill));

  save_and_fill_manager_impl_->OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback_.Get());
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
  // The CVC value should still be filled as long as the user provided it.
  EXPECT_THAT(card_to_fill.cvc(), u"123");
}
#endif  // !BUILDFLAG(IS_IOS)

TEST_F(SaveAndFillManagerImplTest,
       OnDidAcceptCreditCardSaveAndFillSuggestion_ServerSaveAndFill) {
  save_and_fill_manager_impl_->SetCreditCardUploadEnabledOverrideForTesting(
      true);
  UploadCardRequestDetails details;

  EXPECT_CALL(*mock_network_interface_,
              GetDetailsForCreateCard(testing::_, testing::_))
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&details),
                               testing::Return(RequestId("11223344"))));

  save_and_fill_manager_impl_->OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback_.Get());

  EXPECT_EQ(details.upload_card_source,
            UploadCardSource::UPSTREAM_SAVE_AND_FILL);
  EXPECT_EQ(
      details.billing_customer_number,
      payments::GetBillingCustomerId(
          autofill_client_->GetPersonalDataManager().payments_data_manager()));
  EXPECT_EQ(details.app_locale, autofill_client_->GetAppLocale());
  EXPECT_EQ(base::FeatureList::IsEnabled(
                features::kAutofillEnableCvcStorageAndFilling),
            !details.client_behavior_signals.empty());
}

TEST_F(SaveAndFillManagerImplTest, UniqueAddress_SingleAddressCandidate) {
  auto profile = test::GetFullProfile(AddressCountryCode("US"));
  autofill_client_->GetPersonalDataManager()
      .test_address_data_manager()
      .AddProfile(profile);
  save_and_fill_manager_impl_->SetCreditCardUploadEnabledOverrideForTesting(
      true);
  UploadCardRequestDetails details;

  EXPECT_CALL(*mock_network_interface_,
              GetDetailsForCreateCard(testing::_, testing::_))
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&details),
                               testing::Return(RequestId("11223344"))));

  save_and_fill_manager_impl_->OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback_.Get());

  ASSERT_EQ(details.profiles.size(), 1U);
  EXPECT_EQ(details.profiles[0], profile);
}

TEST_F(SaveAndFillManagerImplTest,
       UniqueAddress_MultipleConflictingAddressCandidates) {
  auto& test_address_data_manager =
      autofill_client_->GetPersonalDataManager().test_address_data_manager();
  test_address_data_manager.AddProfile(
      test::GetFullProfile(AddressCountryCode("US")));
  test_address_data_manager.AddProfile(
      test::GetFullProfile2(AddressCountryCode("UK")));
  save_and_fill_manager_impl_->SetCreditCardUploadEnabledOverrideForTesting(
      true);
  UploadCardRequestDetails details;

  EXPECT_CALL(*mock_network_interface_,
              GetDetailsForCreateCard(testing::_, testing::_))
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&details),
                               testing::Return(RequestId("11223344"))));

  save_and_fill_manager_impl_->OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback_.Get());

  EXPECT_TRUE(details.profiles.empty());
}

TEST_F(SaveAndFillManagerImplTest,
       UniqueAddress_MultipleDuplicateAddressCandidates) {
  auto& test_address_data_manager =
      autofill_client_->GetPersonalDataManager().test_address_data_manager();
  auto profile = test::GetFullProfile(AddressCountryCode("US"));
  test_address_data_manager.AddProfile(profile);
  test_address_data_manager.AddProfile(profile);
  save_and_fill_manager_impl_->SetCreditCardUploadEnabledOverrideForTesting(
      true);
  UploadCardRequestDetails details;

  EXPECT_CALL(*mock_network_interface_,
              GetDetailsForCreateCard(testing::_, testing::_))
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&details),
                               testing::Return(RequestId("11223344"))));

  save_and_fill_manager_impl_->OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback_.Get());

  ASSERT_EQ(details.profiles.size(), 1U);
  EXPECT_EQ(details.profiles[0], profile);
}

TEST_F(SaveAndFillManagerImplTest,
       UniqueAddress_NoRecentlyUsedAddressCandidate) {
  constexpr base::Time kJanuary2017 =
      base::Time::FromSecondsSinceUnixEpoch(1484505871);
  auto profile = test::GetFullProfile(AddressCountryCode("US"));
  profile.usage_history().set_modification_date(kJanuary2017);
  profile.usage_history().set_use_date(kJanuary2017);
  autofill_client_->GetPersonalDataManager()
      .test_address_data_manager()
      .AddProfile(profile);
  save_and_fill_manager_impl_->SetCreditCardUploadEnabledOverrideForTesting(
      true);
  UploadCardRequestDetails details;

  EXPECT_CALL(*mock_network_interface_,
              GetDetailsForCreateCard(testing::_, testing::_))
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&details),
                               testing::Return(RequestId("11223344"))));

  save_and_fill_manager_impl_->OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback_.Get());

  EXPECT_TRUE(details.profiles.empty());
}

// Test that the server dialog is shown when the preflight call succeeds and
// legal messages are parsed correctly.
TEST_F(SaveAndFillManagerImplTest,
       OnDidGetDetailsForCreateCard_Success_OfferUploadSaveAndFill) {
  save_and_fill_manager_impl_->SetCreditCardUploadEnabledOverrideForTesting(
      true);
  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/true);

  EXPECT_CALL(*mock_network_interface_,
              GetDetailsForCreateCard(testing::_, testing::_));
  EXPECT_CALL(*payments_autofill_client_,
              ShowCreditCardUploadSaveAndFillDialog);

  save_and_fill_manager_impl_->OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback_.Get());
}

// Test that local Save and Fill is offered as a fallback when legal message
// parsing fails.
TEST_F(
    SaveAndFillManagerImplTest,
    OnDidGetDetailsForCreateCard_LegalMessageFails_FallbackToLocalSaveAndFill) {
  save_and_fill_manager_impl_->SetCreditCardUploadEnabledOverrideForTesting(
      true);
  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/false);

  EXPECT_CALL(*mock_network_interface_,
              GetDetailsForCreateCard(testing::_, testing::_));
  EXPECT_CALL(*payments_autofill_client_, ShowCreditCardLocalSaveAndFillDialog);

  save_and_fill_manager_impl_->OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback_.Get());
}

// Test that local Save and Fill is offered as a fallback when the preflight RPC
// fails.
TEST_F(SaveAndFillManagerImplTest,
       OnDidGetDetailsForCreateCard_RpcFailure_FallbackToLocalSaveAndFill) {
  save_and_fill_manager_impl_->SetCreditCardUploadEnabledOverrideForTesting(
      true);
  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure,
      /*create_valid_legal_message=*/true);

  EXPECT_CALL(*mock_network_interface_,
              GetDetailsForCreateCard(testing::_, testing::_));
  EXPECT_CALL(*payments_autofill_client_, ShowCreditCardLocalSaveAndFillDialog);

  save_and_fill_manager_impl_->OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback_.Get());
}

TEST_F(SaveAndFillManagerImplTest, LoadRiskData) {
  save_and_fill_manager_impl_->SetCreditCardUploadEnabledOverrideForTesting(
      true);
  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/true);

  EXPECT_CALL(*payments_autofill_client_, ShowCreditCardUploadSaveAndFillDialog)
      .WillOnce([](const LegalMessageLines&,
                   TestPaymentsAutofillClient::CardSaveAndFillDialogCallback
                       callback) {
        std::move(callback).Run(CardSaveAndFillDialogUserDecision::kAccepted,
                                UserProvidedCardSaveAndFillDetails());
      });

  base::OnceCallback<void(const std::string&)> risk_data_loaded_callback;
  EXPECT_CALL(*payments_autofill_client_, LoadRiskData)
      .WillOnce([&](base::OnceCallback<void(const std::string&)> callback) {
        risk_data_loaded_callback = std::move(callback);
      });

  UploadCardRequestDetails details;
  EXPECT_CALL(*mock_network_interface_, CreateCard)
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&details),
                               testing::Return(RequestId("11223344"))));

  save_and_fill_manager_impl_->OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback_.Get());

  std::move(risk_data_loaded_callback).Run("some risk data");

  EXPECT_EQ(details.risk_data, "some risk data");
}

}  // namespace autofill::payments
