// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/save_and_fill_manager_impl.h"

#include "base/check_deref.h"
#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_manager/addresses/test_address_data_manager.h"
#include "components/autofill/core/browser/form_import/form_data_importer_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/metrics/payments/save_and_fill_metrics.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/test/mock_multiple_request_payments_network_interface.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/strike_databases/payments/save_and_fill_strike_database.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/strike_database/strike_database_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::A;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

using MockFillCardCallback =
    base::MockCallback<SaveAndFillManagerImpl::FillCardCallback>;
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

  MOCK_METHOD(void,
              CreditCardUploadCompleted,
              (PaymentsRpcResult, std::optional<OnConfirmationClosedCallback>),
              (override));

  MOCK_METHOD(void, HideCreditCardSaveAndFillDialog, (), (override));
};

class MockPaymentsDataManager : public TestPaymentsDataManager {
 public:
  using TestPaymentsDataManager::TestPaymentsDataManager;
  MOCK_METHOD(void,
              AddServerCvc,
              (int64_t instrument_id, const std::u16string& cvc),
              (override));
};
}  // namespace

class SaveAndFillManagerImplTest : public testing::Test {
 public:
  SaveAndFillManagerImplTest() {
    autofill_client().GetPersonalDataManager().set_payments_data_manager(
        std::make_unique<NiceMock<MockPaymentsDataManager>>());
    payments_data_manager().SetPrefService(autofill_client().GetPrefs());
    autofill_client().set_payments_autofill_client(
        std::make_unique<NiceMock<TestPaymentsAutofillClientMock>>(
            &autofill_client()));
    payments_autofill_client().set_multiple_request_payments_network_interface(
        std::make_unique<NiceMock<MockMultipleRequestPaymentsNetworkInterface>>(
            autofill_client().GetURLLoaderFactory(),
            *autofill_client().GetIdentityManager()));
    autofill_client().set_test_strike_database(
        std::make_unique<TestStrikeDatabase>());

    save_and_fill_manager_impl_ =
        std::make_unique<SaveAndFillManagerImpl>(&autofill_client());
  }

  void SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult result,
      bool create_valid_legal_message,
      const std::vector<std::pair<int, int>>& supported_card_bin_ranges = {}) {
    ON_CALL(network_interface(), GetDetailsForCreateCard)
        .WillByDefault([&, result, create_valid_legal_message,
                        supported_card_bin_ranges](
                           const auto& /*request_details*/,
                           base::OnceCallback<void(
                               PaymentsAutofillClient::PaymentsRpcResult,
                               const std::u16string&,
                               std::unique_ptr<base::Value::Dict>,
                               std::vector<std::pair<int, int>>)> callback) {
          FastForwardBy(base::Milliseconds(600));
          std::move(callback).Run(
              result, u"context_token",
              create_valid_legal_message
                  ? std::make_unique<base::Value::Dict>(
                        base::JSONReader::ReadDict(
                            kLegalMessageLines,
                            base::JSON_PARSE_CHROMIUM_EXTENSIONS)
                            .value())
                  : std::make_unique<base::Value::Dict>(
                        base::JSONReader::ReadDict(
                            kInvalidLegalMessageLines,
                            base::JSON_PARSE_CHROMIUM_EXTENSIONS)
                            .value()),
              supported_card_bin_ranges);
          return RequestId("11223344");
        });
  }

  void SetUpCreateCardResponse(PaymentsAutofillClient::PaymentsRpcResult result,
                               const std::string& instrument_id) {
    ON_CALL(network_interface(), CreateCard)
        .WillByDefault([&, result, instrument_id](
                           const UploadCardRequestDetails&,
                           base::OnceCallback<void(
                               PaymentsAutofillClient::PaymentsRpcResult,
                               const std::string&)> callback) {
          FastForwardBy(base::Milliseconds(1000));
          std::move(callback).Run(result, instrument_id);
          return RequestId("11223344");
        });
  }

  void SetUpUploadSaveAndFillDialogDecision(
      CardSaveAndFillDialogUserDecision decision,
      const UserProvidedCardSaveAndFillDetails& details) {
    ON_CALL(payments_autofill_client(), ShowCreditCardUploadSaveAndFillDialog)
        .WillByDefault(
            [decision, details](
                const LegalMessageLines&,
                TestPaymentsAutofillClient::CardSaveAndFillDialogCallback
                    callback) { std::move(callback).Run(decision, details); });
  }

  TestAddressDataManager& address_data_manager() {
    return autofill_client()
        .GetPersonalDataManager()
        .test_address_data_manager();
  }
  TestAutofillClient& autofill_client() { return autofill_client_; }
  MockMultipleRequestPaymentsNetworkInterface& network_interface() {
    return static_cast<MockMultipleRequestPaymentsNetworkInterface&>(
        *payments_autofill_client()
             .GetMultipleRequestPaymentsNetworkInterface());
  }
  TestPaymentsAutofillClientMock& payments_autofill_client() {
    return static_cast<TestPaymentsAutofillClientMock&>(
        CHECK_DEREF(autofill_client().GetPaymentsAutofillClient()));
  }
  MockPaymentsDataManager& payments_data_manager() {
    return static_cast<MockPaymentsDataManager&>(
        autofill_client().GetPersonalDataManager().payments_data_manager());
  }
  SaveAndFillManagerImpl& save_and_fill_manager() {
    return *save_and_fill_manager_impl_;
  }
  TestStrikeDatabase* strike_database() {
    return autofill_client().GetStrikeDatabase();
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void ResetSaveAndFillManager() { save_and_fill_manager_impl_.reset(); }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestAutofillClient autofill_client_;
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
  EXPECT_CALL(payments_autofill_client(),
              ShowCreditCardLocalSaveAndFillDialog(
                  A<PaymentsAutofillClient::CardSaveAndFillDialogCallback>()));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());
}

TEST_F(SaveAndFillManagerImplTest,
       OnDidAcceptCreditCardSaveAndFillSuggestion_NotifyFormDataImporter) {
  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());

  EXPECT_TRUE(autofill_client()
                  .GetFormDataImporter()
                  ->fetched_payments_data_context()
                  .card_submitted_through_save_and_fill);
}

TEST_F(SaveAndFillManagerImplTest, OnUserDidDecideOnLocalSave_Accepted) {
  // Disable StrikeDB check so it will not block feature prompt.
  base::test::ScopedFeatureList feature_list(
      strike_database::features::kDisableStrikeSystem);
  SaveAndFillStrikeDatabase save_and_fill_strike_database(strike_database());
  // Add an existing strike.
  save_and_fill_strike_database.AddStrike();
  EXPECT_EQ(1, save_and_fill_strike_database.GetStrikes());

  EXPECT_CALL(payments_autofill_client(),
              ShowCreditCardLocalSaveAndFillDialog(
                  A<PaymentsAutofillClient::CardSaveAndFillDialogCallback>()));

  CreditCard card_to_fill;
  MockFillCardCallback fill_card_callback;
  EXPECT_CALL(fill_card_callback, Run(A<const CreditCard&>()))
      .WillOnce(SaveArg<0>(&card_to_fill));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback.Get());
  save_and_fill_manager().OnUserDidDecideOnLocalSave(
      CardSaveAndFillDialogUserDecision::kAccepted,
      CreateUserProvidedCardDetails(
          /*card_number=*/u"4444333322221111", /*cardholder_name=*/u"John Doe",
          /*expiration_date_month=*/u"06",
          /*expiration_date_year=*/u"2035",
          /*security_code=*/u"123"));

  EXPECT_THAT(payments_data_manager().GetCreditCards().size(), 1u);

  const CreditCard* saved_card =
      payments_data_manager().GetLocalCreditCards()[0];

  EXPECT_EQ(u"4444333322221111", saved_card->GetRawInfo(CREDIT_CARD_NUMBER));
  EXPECT_EQ(u"John Doe", saved_card->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"06", saved_card->GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(u"2035", saved_card->GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  EXPECT_EQ(u"4444333322221111", card_to_fill.GetRawInfo(CREDIT_CARD_NUMBER));
  EXPECT_EQ(u"John Doe", card_to_fill.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"06", card_to_fill.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(u"2035", card_to_fill.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Make sure that all strikes are cleared upon user acceptance.
  EXPECT_EQ(0, save_and_fill_strike_database.GetStrikes());
}

TEST_F(SaveAndFillManagerImplTest, OnUserDidDecideOnLocalSave_Declined) {
  EXPECT_CALL(payments_autofill_client(),
              ShowCreditCardLocalSaveAndFillDialog(
                  A<PaymentsAutofillClient::CardSaveAndFillDialogCallback>()));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());
  save_and_fill_manager().OnUserDidDecideOnLocalSave(
      CardSaveAndFillDialogUserDecision::kDeclined,
      UserProvidedCardSaveAndFillDetails());

  EXPECT_TRUE(payments_data_manager().GetCreditCards().empty());
}

#if !BUILDFLAG(IS_IOS)
TEST_F(SaveAndFillManagerImplTest, LocallySaveCreditCard_WithCvc_PrefOn) {
  prefs::SetPaymentCvcStorage(autofill_client().GetPrefs(), true);

  EXPECT_CALL(payments_autofill_client(),
              ShowCreditCardLocalSaveAndFillDialog(
                  A<PaymentsAutofillClient::CardSaveAndFillDialogCallback>()));

  CreditCard card_to_fill;
  MockFillCardCallback fill_card_callback;
  EXPECT_CALL(fill_card_callback, Run(A<const CreditCard&>()))
      .WillOnce(SaveArg<0>(&card_to_fill));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback.Get());
  save_and_fill_manager().OnUserDidDecideOnLocalSave(
      CardSaveAndFillDialogUserDecision::kAccepted,
      CreateUserProvidedCardDetails(
          /*card_number=*/u"4444333322221111", /*cardholder_name=*/u"John Doe",
          /*expiration_date_month=*/u"06",
          /*expiration_date_year=*/u"2035",
          /*security_code=*/u"123"));

  EXPECT_THAT(payments_data_manager().GetCreditCards().size(), 1u);
  EXPECT_THAT(payments_data_manager().GetLocalCreditCards().front()->cvc(),
              u"123");
  EXPECT_THAT(card_to_fill.cvc(), u"123");
}

TEST_F(SaveAndFillManagerImplTest, LocallySaveCreditCard_WithCvc_PrefOff) {
  prefs::SetPaymentCvcStorage(autofill_client().GetPrefs(), false);

  EXPECT_CALL(payments_autofill_client(),
              ShowCreditCardLocalSaveAndFillDialog(
                  A<PaymentsAutofillClient::CardSaveAndFillDialogCallback>()));

  CreditCard card_to_fill;
  MockFillCardCallback fill_card_callback;
  EXPECT_CALL(fill_card_callback, Run(A<const CreditCard&>()))
      .WillOnce(SaveArg<0>(&card_to_fill));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback.Get());
  save_and_fill_manager().OnUserDidDecideOnLocalSave(
      CardSaveAndFillDialogUserDecision::kAccepted,
      CreateUserProvidedCardDetails(
          /*card_number=*/u"4444333322221111", /*cardholder_name=*/u"John Doe",
          /*expiration_date_month=*/u"06",
          /*expiration_date_year=*/u"2035",
          /*security_code=*/u"123"));

  EXPECT_THAT(payments_data_manager().GetCreditCards().size(), 1u);
  EXPECT_THAT(payments_data_manager().GetLocalCreditCards().front()->cvc(),
              u"");
  // The CVC value should still be filled as long as the user provided it.
  EXPECT_THAT(card_to_fill.cvc(), u"123");
}
#endif  // !BUILDFLAG(IS_IOS)

TEST_F(SaveAndFillManagerImplTest,
       OnDidAcceptCreditCardSaveAndFillSuggestion_ServerSaveAndFill) {
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  UploadCardRequestDetails details;

  EXPECT_CALL(network_interface(), GetDetailsForCreateCard)
      .WillOnce(DoAll(SaveArg<0>(&details), Return(RequestId("11223344"))));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());

  EXPECT_EQ(details.upload_card_source, UploadCardSource::kUpstreamSaveAndFill);
  EXPECT_EQ(details.billing_customer_number,
            payments::GetBillingCustomerId(payments_data_manager()));
  EXPECT_EQ(details.app_locale, autofill_client().GetAppLocale());
  EXPECT_THAT(
      details.client_behavior_signals,
      Contains(ClientBehaviorConstants::kShowAccountEmailInLegalMessage));
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableCvcStorageAndFilling)) {
    EXPECT_THAT(details.client_behavior_signals,
                Contains(ClientBehaviorConstants::kOfferingToSaveCvc));
  }
}

TEST_F(SaveAndFillManagerImplTest, UniqueAddress_SingleAddressCandidate) {
  auto profile = test::GetFullProfile(AddressCountryCode("US"));
  address_data_manager().AddProfile(profile);
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  UploadCardRequestDetails details;

  EXPECT_CALL(network_interface(), GetDetailsForCreateCard)
      .WillOnce(DoAll(SaveArg<0>(&details), Return(RequestId("11223344"))));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());

  ASSERT_EQ(details.profiles.size(), 1U);
  EXPECT_EQ(details.profiles[0], profile);
}

TEST_F(SaveAndFillManagerImplTest,
       UniqueAddress_MultipleConflictingAddressCandidates) {
  address_data_manager().AddProfile(
      test::GetFullProfile(AddressCountryCode("US")));
  address_data_manager().AddProfile(
      test::GetFullProfile2(AddressCountryCode("UK")));
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  UploadCardRequestDetails details;

  EXPECT_CALL(network_interface(), GetDetailsForCreateCard)
      .WillOnce(DoAll(SaveArg<0>(&details), Return(RequestId("11223344"))));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());

  EXPECT_TRUE(details.profiles.empty());
}

TEST_F(SaveAndFillManagerImplTest,
       UniqueAddress_MultipleDuplicateAddressCandidates) {
  auto profile = test::GetFullProfile(AddressCountryCode("US"));
  address_data_manager().AddProfile(profile);
  address_data_manager().AddProfile(profile);
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  UploadCardRequestDetails details;

  EXPECT_CALL(network_interface(), GetDetailsForCreateCard)
      .WillOnce(DoAll(SaveArg<0>(&details), Return(RequestId("11223344"))));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());

  ASSERT_EQ(details.profiles.size(), 1U);
  EXPECT_EQ(details.profiles[0], profile);
}

TEST_F(SaveAndFillManagerImplTest,
       UniqueAddress_NoRecentlyUsedAddressCandidate) {
  auto profile = test::GetFullProfile(AddressCountryCode("US"));
  profile.usage_history().set_modification_date(base::Time::Now());
  profile.usage_history().set_use_date(base::Time::Now());
  address_data_manager().AddProfile(profile);
  FastForwardBy(base::Days(360));
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  UploadCardRequestDetails details;

  EXPECT_CALL(network_interface(), GetDetailsForCreateCard)
      .WillOnce(DoAll(SaveArg<0>(&details), Return(RequestId("11223344"))));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());

  EXPECT_TRUE(details.profiles.empty());
}

// Test that the server dialog is shown when the preflight call succeeds and
// legal messages are parsed correctly.
TEST_F(SaveAndFillManagerImplTest,
       OnDidGetDetailsForCreateCard_Success_OfferUploadSaveAndFill) {
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/true);

  EXPECT_CALL(network_interface(), GetDetailsForCreateCard);
  EXPECT_CALL(payments_autofill_client(),
              ShowCreditCardUploadSaveAndFillDialog);

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());
}

// Test that local Save and Fill is offered as a fallback when legal message
// parsing fails.
TEST_F(
    SaveAndFillManagerImplTest,
    OnDidGetDetailsForCreateCard_LegalMessageFails_FallbackToLocalSaveAndFill) {
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/false);

  EXPECT_CALL(network_interface(), GetDetailsForCreateCard);
  EXPECT_CALL(payments_autofill_client(), ShowCreditCardLocalSaveAndFillDialog);

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());
}

// Test that local Save and Fill is offered as a fallback when the preflight RPC
// fails.
TEST_F(SaveAndFillManagerImplTest,
       OnDidGetDetailsForCreateCard_RpcFailure_FallbackToLocalSaveAndFill) {
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure,
      /*create_valid_legal_message=*/true);

  EXPECT_CALL(network_interface(), GetDetailsForCreateCard);
  EXPECT_CALL(payments_autofill_client(), ShowCreditCardLocalSaveAndFillDialog);

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());
}

TEST_F(SaveAndFillManagerImplTest, LoadRiskData) {
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/true);

  auto user_provided_details = CreateUserProvidedCardDetails(
      /*card_number=*/u"1111222233334444",
      /*cardholder_name=*/u"Jane Smith",
      /*expiration_date_month=*/u"06",
      /*expiration_date_year=*/u"2035",
      /*security_code=*/u"456");
  SetUpUploadSaveAndFillDialogDecision(
      CardSaveAndFillDialogUserDecision::kAccepted, user_provided_details);

  base::OnceCallback<void(const std::string&)> risk_data_loaded_callback;
  EXPECT_CALL(payments_autofill_client(), LoadRiskData)
      .WillOnce(MoveArg(&risk_data_loaded_callback));

  UploadCardRequestDetails details;
  EXPECT_CALL(network_interface(), CreateCard)
      .WillOnce(DoAll(SaveArg<0>(&details), Return(RequestId("11223344"))));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());
  std::move(risk_data_loaded_callback).Run("some risk data");

  EXPECT_EQ(details.risk_data, "some risk data");
}

TEST_F(SaveAndFillManagerImplTest,
       OnUserDidDecideOnLocalSave_Declined_AddsStrike) {
  SaveAndFillStrikeDatabase save_and_fill_strike_database(strike_database());

  save_and_fill_manager().OnUserDidDecideOnLocalSave(
      CardSaveAndFillDialogUserDecision::kDeclined,
      UserProvidedCardSaveAndFillDetails());

  EXPECT_EQ(1, save_and_fill_strike_database.GetStrikes());
}

TEST_F(SaveAndFillManagerImplTest,
       OnUserDidDecideOnUploadSave_Declined_AddsStrike) {
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/true);
  SaveAndFillStrikeDatabase save_and_fill_strike_database(strike_database());
  EXPECT_CALL(payments_autofill_client(), ShowCreditCardUploadSaveAndFillDialog)
      .WillOnce(RunOnceCallback<1>(CardSaveAndFillDialogUserDecision::kDeclined,
                                   UserProvidedCardSaveAndFillDetails()));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());
  EXPECT_EQ(1, save_and_fill_strike_database.GetStrikes());
}

TEST_F(SaveAndFillManagerImplTest, OnUserDidDecideOnUploadSave_Accepted) {
  // Disable StrikeDB check so it will not block feature prompt.
  base::test::ScopedFeatureList feature_list(
      strike_database::features::kDisableStrikeSystem);
  SaveAndFillStrikeDatabase save_and_fill_strike_database(strike_database());
  // Add an existing strike.
  save_and_fill_strike_database.AddStrike();
  EXPECT_EQ(1, save_and_fill_strike_database.GetStrikes());

  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/true);

  CreditCard card_to_fill;
  MockFillCardCallback fill_card_callback;
  EXPECT_CALL(fill_card_callback, Run(A<const CreditCard&>()))
      .WillOnce(SaveArg<0>(&card_to_fill));
  EXPECT_CALL(payments_autofill_client(), ShowCreditCardUploadSaveAndFillDialog)
      .WillOnce(RunOnceCallback<1>(CardSaveAndFillDialogUserDecision::kAccepted,
                                   CreateUserProvidedCardDetails(
                                       /*card_number=*/u"1111222233334444",
                                       /*cardholder_name=*/u"Jane Smith",
                                       /*expiration_date_month=*/u"06",
                                       /*expiration_date_year=*/u"2035",
                                       /*security_code=*/u"456")));

  EXPECT_CALL(payments_autofill_client(), LoadRiskData)
      .WillOnce(RunOnceCallback<0>("some risk data"));
  EXPECT_CALL(network_interface(), CreateCard)
      .WillOnce(Return(RequestId("11223344")));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback.Get());
  save_and_fill_manager().OnDidCreateCard(
      base::TimeTicks::Now(),
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*instrument_id=*/"1122334455");

  EXPECT_EQ(u"1111222233334444", card_to_fill.GetRawInfo(CREDIT_CARD_NUMBER));
  EXPECT_EQ(u"Jane Smith", card_to_fill.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"06", card_to_fill.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(u"2035", card_to_fill.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_EQ(u"456", card_to_fill.cvc());

  // Make sure that all strikes are cleared upon user acceptance.
  EXPECT_EQ(0, save_and_fill_strike_database.GetStrikes());
}

TEST_F(SaveAndFillManagerImplTest, CardUploadFeedback_UploadSucceeded) {
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);

  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/true);

  auto user_provided_details = CreateUserProvidedCardDetails(
      /*card_number=*/u"1111222233334444",
      /*cardholder_name=*/u"Jane Smith",
      /*expiration_date_month=*/u"06",
      /*expiration_date_year=*/u"2035",
      /*security_code=*/u"456");
  SetUpUploadSaveAndFillDialogDecision(
      CardSaveAndFillDialogUserDecision::kAccepted, user_provided_details);

  EXPECT_CALL(payments_autofill_client(), LoadRiskData)
      .WillOnce(RunOnceCallback<0>("some risk data"));

  SetUpCreateCardResponse(PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
                          "112233445566L");

  EXPECT_CALL(payments_autofill_client(),
              CreditCardUploadCompleted(
                  PaymentsAutofillClient::PaymentsRpcResult::kSuccess, _));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());
}

TEST_F(SaveAndFillManagerImplTest, CardUploadFeedback_UploadFailed) {
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);

  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/true);

  auto user_provided_details = CreateUserProvidedCardDetails(
      /*card_number=*/u"1111222233334444",
      /*cardholder_name=*/u"Jane Smith",
      /*expiration_date_month=*/u"06",
      /*expiration_date_year=*/u"2035",
      /*security_code=*/u"456");
  SetUpUploadSaveAndFillDialogDecision(
      CardSaveAndFillDialogUserDecision::kAccepted, user_provided_details);

  EXPECT_CALL(payments_autofill_client(), LoadRiskData)
      .WillOnce(RunOnceCallback<0>("some risk data"));

  SetUpCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure, "");

  EXPECT_CALL(
      payments_autofill_client(),
      CreditCardUploadCompleted(
          PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure, _));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());

  std::vector<const CreditCard*> cards =
      payments_data_manager().GetLocalCreditCards();
  ASSERT_EQ(cards.size(), 1U);
  EXPECT_EQ(cards[0]->number(), u"1111222233334444");
}

// Verify that a strike is added when the suggestion is offered but not
// selected, and the form is submitted.
TEST_F(SaveAndFillManagerImplTest,
       OnFormSubmitted_AddsStrikeWhenSuggestionOfferedButNotSelected) {
  SaveAndFillStrikeDatabase save_and_fill_strike_database(strike_database());

  save_and_fill_manager().OnSuggestionOffered();
  save_and_fill_manager().MaybeAddStrikeForSaveAndFill();

  EXPECT_EQ(1, save_and_fill_strike_database.GetStrikes());

  // Verifies that calling it again won't log another strike.
  save_and_fill_manager().MaybeAddStrikeForSaveAndFill();
  EXPECT_EQ(1, save_and_fill_strike_database.GetStrikes());
}

// Verify that no strike is added if the suggestion was offered and accepted by
// the user.
TEST_F(SaveAndFillManagerImplTest,
       OnFormSubmitted_NoStrikeWhenSuggestionOfferedAndSelected) {
  SaveAndFillStrikeDatabase save_and_fill_strike_database(strike_database());

  save_and_fill_manager().OnSuggestionOffered();
  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());
  save_and_fill_manager().MaybeAddStrikeForSaveAndFill();

  EXPECT_EQ(0, save_and_fill_strike_database.GetStrikes());
}

// Verify that no strike is added if the suggestion is offered but the form is
// never submitted.
TEST_F(SaveAndFillManagerImplTest,
       OnFormSubmitted_NoStrikeWhenFormNotSubmitted) {
  SaveAndFillStrikeDatabase save_and_fill_strike_database(strike_database());

  save_and_fill_manager().OnSuggestionOffered();
  // To simulate the tab being closed, we destroy the save and fill manager.
  ResetSaveAndFillManager();

  EXPECT_EQ(0, save_and_fill_strike_database.GetStrikes());
}

TEST_F(SaveAndFillManagerImplTest, RequestLatencyMetrics) {
  base::HistogramTester histogram_tester;

  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);

  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/true);

  auto user_provided_details = CreateUserProvidedCardDetails(
      /*card_number=*/u"1111222233334444",
      /*cardholder_name=*/u"Jane Smith",
      /*expiration_date_month=*/u"06",
      /*expiration_date_year=*/u"2035",
      /*security_code=*/u"456");
  SetUpUploadSaveAndFillDialogDecision(
      CardSaveAndFillDialogUserDecision::kAccepted, user_provided_details);

  EXPECT_CALL(payments_autofill_client(), LoadRiskData)
      .WillOnce(RunOnceCallback<0>("some risk data"));

  SetUpCreateCardResponse(PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
                          "112233445566L");

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveAndFill.GetDetailsForCreateCard.Latency", 600, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveAndFill.GetDetailsForCreateCard.Latency.Success", 600, 1);
  histogram_tester.ExpectUniqueSample("Autofill.SaveAndFill.CreateCard.Latency",
                                      1000, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveAndFill.CreateCard.Latency.Success", 1000, 1);
}

TEST_F(SaveAndFillManagerImplTest, ResetOnFlowEnds_ServerSave) {
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/true);

  auto user_provided_details = CreateUserProvidedCardDetails(
      /*card_number=*/u"1111222233334444",
      /*cardholder_name=*/u"Jane Smith",
      /*expiration_date_month=*/u"06",
      /*expiration_date_year=*/u"2035",
      /*security_code=*/u"456");
  ON_CALL(payments_autofill_client(), ShowCreditCardUploadSaveAndFillDialog)
      .WillByDefault(
          [&](const LegalMessageLines&,
              TestPaymentsAutofillClient::CardSaveAndFillDialogCallback
                  callback) {
            std::move(callback).Run(
                CardSaveAndFillDialogUserDecision::kAccepted,
                user_provided_details);
            EXPECT_TRUE(
                save_and_fill_manager().save_and_fill_suggestion_selected_);
            EXPECT_EQ(save_and_fill_manager().upload_details_.card.number(),
                      u"1111222233334444");
          });
  ON_CALL(payments_autofill_client(), LoadRiskData)
      .WillByDefault(RunOnceCallback<0>("some risk data"));

  save_and_fill_manager().OnSuggestionOffered();
  EXPECT_TRUE(save_and_fill_manager().save_and_fill_suggestion_offered_);

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());
  save_and_fill_manager().OnDidCreateCard(
      base::TimeTicks::Now(),
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*instrument_id=*/"1122334455");

  // Verifies that the states variable in the SaveAndFillManagerImpl get reset
  // when flow ends.
  EXPECT_FALSE(save_and_fill_manager().weak_ptr_factory_.HasWeakPtrs());
  EXPECT_FALSE(save_and_fill_manager().upload_save_and_fill_dialog_accepted_);
  EXPECT_FALSE(save_and_fill_manager().save_and_fill_suggestion_offered_);
  EXPECT_FALSE(save_and_fill_manager().save_and_fill_suggestion_selected_);
  EXPECT_TRUE(save_and_fill_manager().fill_card_callback_.is_null());
  EXPECT_TRUE(save_and_fill_manager().upload_details_.card.number().empty());
}

TEST_F(SaveAndFillManagerImplTest, ResetOnFlowEnds_LocalSave) {
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(false);

  save_and_fill_manager().OnSuggestionOffered();
  EXPECT_TRUE(save_and_fill_manager().save_and_fill_suggestion_offered_);

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());
  EXPECT_TRUE(save_and_fill_manager().save_and_fill_suggestion_selected_);
  EXPECT_FALSE(save_and_fill_manager().fill_card_callback_.is_null());

  save_and_fill_manager().OnUserDidDecideOnLocalSave(
      CardSaveAndFillDialogUserDecision::kAccepted,
      CreateUserProvidedCardDetails(
          /*card_number=*/u"4444333322221111", /*cardholder_name=*/u"John Doe",
          /*expiration_date_month=*/u"06",
          /*expiration_date_year=*/u"2035",
          /*security_code=*/u"123"));

  // Verifies that the states variable in the SaveAndFillManagerImpl get reset
  // when flow ends.
  EXPECT_FALSE(save_and_fill_manager().weak_ptr_factory_.HasWeakPtrs());
  EXPECT_FALSE(save_and_fill_manager().save_and_fill_suggestion_offered_);
  EXPECT_FALSE(save_and_fill_manager().save_and_fill_suggestion_selected_);
  EXPECT_TRUE(save_and_fill_manager().fill_card_callback_.is_null());
}

TEST_F(SaveAndFillManagerImplTest, StrikeDatabaseMetrics) {
  base::HistogramTester histogram_tester;
  SaveAndFillStrikeDatabase save_and_fill_strike_database(strike_database());

  save_and_fill_strike_database.AddStrike();

  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NthStrikeAdded.SaveAndFill",
      /*sample=*/1, /*expected_bucket_count=*/1);

  EXPECT_EQ(save_and_fill_manager().ShouldBlockFeature(), true);
  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.SaveAndFillStrikeDatabaseBlockReason",
      /*sample=*/1, 1);

  save_and_fill_strike_database.AddStrikes(
      save_and_fill_strike_database.GetMaxStrikesLimit() - 1);

  EXPECT_EQ(save_and_fill_manager().ShouldBlockFeature(), true);
  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.SaveAndFillStrikeDatabaseBlockReason",
      /*sample=*/0, 1);

  save_and_fill_strike_database.RemoveStrikes(1);
  save_and_fill_manager().OnUserDidDecideOnLocalSave(
      CardSaveAndFillDialogUserDecision::kAccepted,
      CreateUserProvidedCardDetails(
          /*card_number=*/u"4444333322221111", /*cardholder_name=*/u"John Doe",
          /*expiration_date_month=*/u"06",
          /*expiration_date_year=*/u"2035",
          /*security_code=*/u"123"));

  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NumOfStrikesPresentWhenSaveAndFillAccepted",
      /*sample=*/2,
      /*expected_bucket_count=*/1);
}

TEST_F(SaveAndFillManagerImplTest, HideDialog_CalledAfterLocalSaveCompleted) {
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(false);

  EXPECT_CALL(payments_autofill_client(),
              ShowCreditCardLocalSaveAndFillDialog(
                  A<PaymentsAutofillClient::CardSaveAndFillDialogCallback>()));
  EXPECT_CALL(payments_autofill_client(), HideCreditCardSaveAndFillDialog())
      .Times(1);

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());
  save_and_fill_manager().OnUserDidDecideOnLocalSave(
      CardSaveAndFillDialogUserDecision::kAccepted,
      CreateUserProvidedCardDetails(
          /*card_number=*/u"4444333322221111", /*cardholder_name=*/u"John Doe",
          /*expiration_date_month=*/u"06",
          /*expiration_date_year=*/u"2035",
          /*security_code=*/u"123"));
}

TEST_F(SaveAndFillManagerImplTest, HideDialog_CalledAfterServerSaveCompleted) {
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/true);

  auto user_provided_details = CreateUserProvidedCardDetails(
      /*card_number=*/u"1111222233334444",
      /*cardholder_name=*/u"Jane Smith",
      /*expiration_date_month=*/u"06",
      /*expiration_date_year=*/u"2035",
      /*security_code=*/u"456");
  SetUpUploadSaveAndFillDialogDecision(
      CardSaveAndFillDialogUserDecision::kAccepted, user_provided_details);

  EXPECT_CALL(payments_autofill_client(), LoadRiskData)
      .WillOnce(RunOnceCallback<0>("some risk data"));

  SetUpCreateCardResponse(PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
                          "112233445566L");
  EXPECT_CALL(payments_autofill_client(), HideCreditCardSaveAndFillDialog());

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());
}

TEST_F(SaveAndFillManagerImplTest, OnDidCreateCard_Success_SaveServerCvc) {
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  prefs::SetPaymentCvcStorage(autofill_client().GetPrefs(), true);
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnableCvcStorageAndFilling);
  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/true);
  SetUpUploadSaveAndFillDialogDecision(
      CardSaveAndFillDialogUserDecision::kAccepted,
      CreateUserProvidedCardDetails(
          /*card_number=*/u"1111222233334444",
          /*cardholder_name=*/u"Jane Smith",
          /*expiration_date_month=*/u"06",
          /*expiration_date_year=*/u"2035",
          /*security_code=*/u"456"));

  EXPECT_CALL(payments_autofill_client(), LoadRiskData)
      .WillOnce(RunOnceCallback<0>("some risk data"));

  SetUpCreateCardResponse(PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
                          "112233445566");
  EXPECT_CALL(payments_data_manager(),
              AddServerCvc(112233445566L, std::u16string(u"456")));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());
}

TEST_F(SaveAndFillManagerImplTest,
       OnDidCreateCard_Success_DoNotAddServerCvcIfCvcIsEmpty) {
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  prefs::SetPaymentCvcStorage(autofill_client().GetPrefs(), true);
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnableCvcStorageAndFilling);
  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/true);
  SetUpUploadSaveAndFillDialogDecision(
      CardSaveAndFillDialogUserDecision::kAccepted,
      CreateUserProvidedCardDetails(
          /*card_number=*/u"1111222233334444",
          /*cardholder_name=*/u"Jane Smith",
          /*expiration_date_month=*/u"06",
          /*expiration_date_year=*/u"2035",
          /*security_code=*/u""));

  EXPECT_CALL(payments_autofill_client(), LoadRiskData)
      .WillOnce(RunOnceCallback<0>("some risk data"));

  SetUpCreateCardResponse(PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
                          "112233445566");
  EXPECT_CALL(payments_data_manager(), AddServerCvc).Times(0);

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());
}

TEST_F(SaveAndFillManagerImplTest,
       OnDidCreateCard_Success_DoNotSaveServerCvcIfCvcStorageIsDisabled) {
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  prefs::SetPaymentCvcStorage(autofill_client().GetPrefs(), false);
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnableCvcStorageAndFilling);
  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/true);
  SetUpUploadSaveAndFillDialogDecision(
      CardSaveAndFillDialogUserDecision::kAccepted,
      CreateUserProvidedCardDetails(
          /*card_number=*/u"1111222233334444",
          /*cardholder_name=*/u"Jane Smith",
          /*expiration_date_month=*/u"06",
          /*expiration_date_year=*/u"2035",
          /*security_code=*/u"456"));

  EXPECT_CALL(payments_autofill_client(), LoadRiskData)
      .WillOnce(RunOnceCallback<0>("some risk data"));

  SetUpCreateCardResponse(PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
                          "112233445566");
  EXPECT_CALL(payments_data_manager(), AddServerCvc).Times(0);

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());
}

TEST_F(SaveAndFillManagerImplTest, LogFunnelMetrics_ServerSave) {
  base::HistogramTester histogram_tester;

  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/true);

  EXPECT_CALL(payments_autofill_client(), ShowCreditCardUploadSaveAndFillDialog)
      .WillOnce(RunOnceCallback<1>(CardSaveAndFillDialogUserDecision::kAccepted,
                                   CreateUserProvidedCardDetails(
                                       /*card_number=*/u"1111222233334444",
                                       /*cardholder_name=*/u"Jane Smith",
                                       /*expiration_date_month=*/u"06",
                                       /*expiration_date_year=*/u"2035",
                                       /*security_code=*/u"456")));
  EXPECT_CALL(payments_autofill_client(), LoadRiskData)
      .WillOnce(RunOnceCallback<0>("some risk data"));
  EXPECT_CALL(network_interface(), CreateCard)
      .WillOnce(Return(RequestId("11223344")));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());
  save_and_fill_manager().OnDidCreateCard(
      base::TimeTicks::Now(),
      PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure,
      /*instrument_id=*/"");

  save_and_fill_manager().LogCreditCardFormFilled();
  save_and_fill_manager().LogCreditCardFormSubmitted();

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveAndFill.Funnel.Upload.Failure",
      autofill_metrics::SaveAndFillFormEvent::kFormFilled,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Autofill.SaveAndFill.Funnel.Upload.Failure",
      autofill_metrics::SaveAndFillFormEvent::kFormSubmitted,
      /*expected_count=*/1);

  // Make sure calling it multiple times has no effect.
  save_and_fill_manager().LogCreditCardFormFilled();
  save_and_fill_manager().LogCreditCardFormSubmitted();

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveAndFill.Funnel.Upload.Failure",
      autofill_metrics::SaveAndFillFormEvent::kFormFilled,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Autofill.SaveAndFill.Funnel.Upload.Failure",
      autofill_metrics::SaveAndFillFormEvent::kFormSubmitted,
      /*expected_count=*/1);
}

TEST_F(SaveAndFillManagerImplTest, LogFunnelMetrics_LocalSave) {
  base::HistogramTester histogram_tester;
  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      base::DoNothing());
  save_and_fill_manager().OnUserDidDecideOnLocalSave(
      CardSaveAndFillDialogUserDecision::kAccepted,
      CreateUserProvidedCardDetails(
          /*card_number=*/u"1111222233334444",
          /*cardholder_name=*/u"Jane Smith",
          /*expiration_date_month=*/u"06",
          /*expiration_date_year=*/u"2035",
          /*security_code=*/u"456"));

  save_and_fill_manager().LogCreditCardFormFilled();
  save_and_fill_manager().LogCreditCardFormSubmitted();

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveAndFill.Funnel.Local.Success",
      autofill_metrics::SaveAndFillFormEvent::kFormFilled,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Autofill.SaveAndFill.Funnel.Local.Success",
      autofill_metrics::SaveAndFillFormEvent::kFormSubmitted,
      /*expected_count=*/1);

  // Make sure calling it multiple times has no effect.
  save_and_fill_manager().LogCreditCardFormFilled();
  save_and_fill_manager().LogCreditCardFormSubmitted();

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveAndFill.Funnel.Local.Success",
      autofill_metrics::SaveAndFillFormEvent::kFormFilled,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Autofill.SaveAndFill.Funnel.Local.Success",
      autofill_metrics::SaveAndFillFormEvent::kFormSubmitted,
      /*expected_count=*/1);
}

// Test that if the user enters a card with a BIN that is not in the
// supported BIN ranges returned by the server, the upload flow is terminated
// and local save is offered instead as a fallback.
TEST_F(SaveAndFillManagerImplTest,
       UnsupportedBinRange_TriggersLocalSaveFallback) {
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/true,
      /*supported_card_bin_ranges=*/{{400000, 499999}});

  auto user_provided_details = CreateUserProvidedCardDetails(
      /*card_number=*/u"5454545454545454",
      /*cardholder_name=*/u"Jane Smith",
      /*expiration_date_month=*/u"06",
      /*expiration_date_year=*/u"2035",
      /*security_code=*/u"456");
  SetUpUploadSaveAndFillDialogDecision(
      CardSaveAndFillDialogUserDecision::kAccepted, user_provided_details);

  EXPECT_CALL(network_interface(), CreateCard).Times(0);
  MockFillCardCallback fill_card_callback;
  EXPECT_CALL(fill_card_callback, Run(A<const CreditCard&>()));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback.Get());

  std::vector<const CreditCard*> cards =
      payments_data_manager().GetLocalCreditCards();
  ASSERT_EQ(cards.size(), 1U);
  EXPECT_EQ(cards[0]->number(), u"5454545454545454");
}

// Test that if the user enters a card with a supported BIN, the upload flow
// proceeds as normal.
TEST_F(SaveAndFillManagerImplTest, UploadSaveOfferedForSupportedBinCard) {
  save_and_fill_manager().SetCreditCardUploadEnabledOverrideForTesting(true);
  SetUpGetDetailsForCreateCardResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*create_valid_legal_message=*/true,
      /*supported_card_bin_ranges=*/{{4111, 4111}});

  auto user_provided_details = CreateUserProvidedCardDetails(
      /*card_number=*/u"4111111111111111",
      /*cardholder_name=*/u"Jane Smith",
      /*expiration_date_month=*/u"06",
      /*expiration_date_year=*/u"2035",
      /*security_code=*/u"456");
  SetUpUploadSaveAndFillDialogDecision(
      CardSaveAndFillDialogUserDecision::kAccepted, user_provided_details);

  SetUpCreateCardResponse(PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
                          "112233445566L");

  ON_CALL(payments_autofill_client(), LoadRiskData)
      .WillByDefault(RunOnceCallback<0>("some risk data"));

  MockFillCardCallback fill_card_callback;
  EXPECT_CALL(fill_card_callback, Run(A<const CreditCard&>()));
  EXPECT_CALL(payments_autofill_client(),
              CreditCardUploadCompleted(
                  PaymentsAutofillClient::PaymentsRpcResult::kSuccess, _));

  save_and_fill_manager().OnDidAcceptCreditCardSaveAndFillSuggestion(
      fill_card_callback.Get());

  EXPECT_TRUE(payments_data_manager().GetLocalCreditCards().empty());
}

}  // namespace autofill::payments
