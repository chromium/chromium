// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/native_account_linking_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/mock_facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strike_database/simple_strike_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

using ::testing::_;
using ::testing::Return;

struct TestNativeAccountLinkingStrikeDatabaseTraits {
  static constexpr std::string_view kName = "TestNativeAccountLinking";
  static constexpr std::optional<size_t> kMaxStrikeEntities = std::nullopt;
  static constexpr std::optional<size_t> kMaxStrikeEntitiesAfterCleanup =
      std::nullopt;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr std::optional<base::TimeDelta> kExpiryTimeDelta =
      std::nullopt;
  static constexpr bool kUniqueIdRequired = false;
};

class TestNativeAccountLinkingStrikeDatabase
    : public strike_database::SimpleStrikeDatabase<
          TestNativeAccountLinkingStrikeDatabaseTraits> {
 public:
  using SimpleStrikeDatabase::SimpleStrikeDatabase;

  std::optional<base::TimeDelta> GetRequiredDelaySinceLastStrike()
      const override {
    return base::Days(7);
  }
};

class TestNativeAccountLinkingHandler : public NativeAccountLinkingHandler {
 public:
  using NativeAccountLinkingHandler::FetchClientToken;
  using NativeAccountLinkingHandler::InitiateAccountLinkingNetworkCall;
  using NativeAccountLinkingHandler::ShowAccountLinkingPrompt;

  TestNativeAccountLinkingHandler(
      FacilitatedPaymentsClient* client,
      FacilitatedPaymentsApiClientCreator api_client_creator)
      : NativeAccountLinkingHandler(client, std::move(api_client_creator)) {}
  ~TestNativeAccountLinkingHandler() override = default;

  MOCK_METHOD(void,
              DoOnClientTokenReceived,
              (const std::vector<uint8_t>& client_token),
              (override));
  MOCK_METHOD(void,
              DoOnAccountLinkingResult,
              (AccountLinkingResult result),
              (override));
  MOCK_METHOD(void,
              DoOnGetDetailsForCreatePaymentInstrumentResponse,
              (bool is_eligible),
              (override));
  MOCK_METHOD(std::optional<AccountLinkingParams>,
              CreateAccountLinkingParams,
              (),
              (override));

  strike_database::StrikeDatabaseIntegratorBase* GetStrikeDatabase() override {
    return strike_database_;
  }

  bool IsUserPrefEnabled() const override { return is_user_pref_enabled_; }

  void set_strike_database(
      strike_database::StrikeDatabaseIntegratorBase* strike_db) {
    strike_database_ = strike_db;
  }

  void set_is_user_pref_enabled(bool enabled) {
    is_user_pref_enabled_ = enabled;
  }

  std::string_view GetHistogramSuffix() const override { return "TestFop"; }

  base::DictValue GetPayloadForGetDetailsForCreatePaymentInstrument() override {
    return base::DictValue();
  }

  base::WeakPtr<NativeAccountLinkingHandler> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  bool is_prompt_showing() const { return is_prompt_showing_; }

 private:
  raw_ptr<strike_database::StrikeDatabaseIntegratorBase> strike_database_ =
      nullptr;
  bool is_user_pref_enabled_ = true;
  base::WeakPtrFactory<TestNativeAccountLinkingHandler> weak_ptr_factory_{this};
};

class NativeAccountLinkingHandlerTest : public testing::Test {
 public:
  NativeAccountLinkingHandlerTest() {
    ON_CALL(client_, GetPaymentsDataManager)
        .WillByDefault(Return(&payments_data_manager_));
    ON_CALL(client_, GetFacilitatedPaymentsNetworkInterface)
        .WillByDefault(Return(&payments_network_interface_));
  }

  void SetUp() override {
    pref_service_ = autofill::test::PrefServiceForTesting();
    payments_data_manager_.SetPrefService(pref_service_.get());
    payments_data_manager_.SetPaymentsCustomerData(
        std::make_unique<autofill::PaymentsCustomerData>("123456"));
    payments_data_manager_.SetAccountInfoForPayments(
        identity_test_env_.MakePrimaryAccountAvailable(
            "test@example.com", signin::ConsentLevel::kSignin));
    ON_CALL(client_, GetCoreAccountInfo)
        .WillByDefault(
            Return(payments_data_manager_.GetAccountInfoForPaymentsServer()));

    test_strike_database_service_ =
        std::make_unique<autofill::TestStrikeDatabase>();
    test_strike_database_ =
        std::make_unique<TestNativeAccountLinkingStrikeDatabase>(
            test_strike_database_service_.get());

    api_client_ = std::make_unique<MockFacilitatedPaymentsApiClient>();
    api_client_ptr_ = api_client_.get();

    handler_ = std::make_unique<TestNativeAccountLinkingHandler>(
        &client_,
        base::BindRepeating(&NativeAccountLinkingHandlerTest::CreateApiClient,
                            base::Unretained(this)));
    handler_->set_strike_database(test_strike_database_.get());
  }

  void TearDown() override {
    payments_data_manager_.ClearAllServerDataForTesting();
  }

  std::unique_ptr<FacilitatedPaymentsApiClient> CreateApiClient() {
    return std::move(api_client_);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockFacilitatedPaymentsClient client_;
  autofill::TestPaymentsDataManager payments_data_manager_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<autofill::TestStrikeDatabase> test_strike_database_service_;
  std::unique_ptr<TestNativeAccountLinkingStrikeDatabase> test_strike_database_;
  MockFacilitatedPaymentsNetworkInterface payments_network_interface_{
      *identity_test_env_.identity_manager(), payments_data_manager_};

  std::unique_ptr<MockFacilitatedPaymentsApiClient> api_client_;
  raw_ptr<MockFacilitatedPaymentsApiClient> api_client_ptr_ = nullptr;

  std::unique_ptr<TestNativeAccountLinkingHandler> handler_;
  base::HistogramTester histogram_tester_;
};

TEST_F(NativeAccountLinkingHandlerTest, FetchClientToken_Success) {
  std::vector<uint8_t> expected_token = {1, 2, 3};
  EXPECT_CALL(*api_client_ptr_, GetClientToken(_))
      .WillOnce([&](base::OnceCallback<void(std::vector<uint8_t>)> callback) {
        std::move(callback).Run(expected_token);
      });
  EXPECT_CALL(*handler_, DoOnClientTokenReceived(expected_token)).Times(1);

  handler_->FetchClientToken();

  histogram_tester_.ExpectTotalCount(
      "FacilitatedPayments.TestFop.AccountLinking.GetClientToken.Success."
      "Latency",
      1);
  histogram_tester_.ExpectTotalCount(
      "FacilitatedPayments.TestFop.AccountLinking.GetClientToken.Failure."
      "Latency",
      0);
}

TEST_F(NativeAccountLinkingHandlerTest, FetchClientToken_Failure) {
  EXPECT_CALL(*api_client_ptr_, GetClientToken(_))
      .WillOnce([&](base::OnceCallback<void(std::vector<uint8_t>)> callback) {
        std::move(callback).Run({});
      });
  EXPECT_CALL(*handler_, DoOnClientTokenReceived(_)).Times(0);
  EXPECT_CALL(*handler_,
              DoOnAccountLinkingResult(AccountLinkingResult{
                  false, 0, AccountLinkingResultCode::kCouldNotInvoke}))
      .Times(1);

  handler_->FetchClientToken();

  histogram_tester_.ExpectTotalCount(
      "FacilitatedPayments.TestFop.AccountLinking.GetClientToken.Success."
      "Latency",
      0);
  histogram_tester_.ExpectTotalCount(
      "FacilitatedPayments.TestFop.AccountLinking.GetClientToken.Failure."
      "Latency",
      1);
}

TEST_F(NativeAccountLinkingHandlerTest,
       InitiateAccountLinkingNetworkCall_Success) {
  std::vector<uint8_t> client_token = {1, 2, 3};
  EXPECT_CALL(payments_network_interface_,
              GetDetailsForCreatePaymentInstrument(_, client_token, _, _))
      .WillOnce([](long billing_customer_id, const std::vector<uint8_t>& token,
                   auto callback, const std::string& app_locale) {
        std::move(callback).Run(
            autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
                kSuccess,
            /*is_eligible=*/true,
            /*action_token=*/std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'});
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });

  EXPECT_CALL(*handler_, DoOnGetDetailsForCreatePaymentInstrumentResponse(true))
      .Times(1);

  handler_->InitiateAccountLinkingNetworkCall(client_token);

  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.TestFop.AccountLinking."
      "GetDetailsForCreatePaymentInstrument.Result",
      /*sample=*/true, /*expected_bucket_count=*/1);
  histogram_tester_.ExpectTotalCount(
      "FacilitatedPayments.TestFop.AccountLinking."
      "GetDetailsForCreatePaymentInstrument.Latency",
      1);
}

TEST_F(NativeAccountLinkingHandlerTest,
       InitiateAccountLinkingNetworkCall_Failure) {
  std::vector<uint8_t> client_token = {1, 2, 3};
  EXPECT_CALL(payments_network_interface_,
              GetDetailsForCreatePaymentInstrument(_, client_token, _, _))
      .WillOnce([](long billing_customer_id, const std::vector<uint8_t>& token,
                   auto callback, const std::string& app_locale) {
        std::move(callback).Run(autofill::payments::PaymentsAutofillClient::
                                    PaymentsRpcResult::kPermanentFailure,
                                /*is_eligible=*/false,
                                /*action_token=*/std::vector<uint8_t>{});
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });
  EXPECT_CALL(*handler_,
              DoOnAccountLinkingResult(AccountLinkingResult{
                  false, 0, AccountLinkingResultCode::kCouldNotInvoke}))
      .Times(1);

  handler_->InitiateAccountLinkingNetworkCall(client_token);

  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.TestFop.AccountLinking."
      "GetDetailsForCreatePaymentInstrument.Result",
      /*sample=*/false, /*expected_bucket_count=*/1);
  histogram_tester_.ExpectTotalCount(
      "FacilitatedPayments.TestFop.AccountLinking."
      "GetDetailsForCreatePaymentInstrument.Latency",
      1);
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.TestFop.AccountLinking.FlowExitedReason",
      /*sample=*/AccountLinkingFlowExitedReason::kGetDetailsFailed,
      /*expected_bucket_count=*/1);
}

TEST_F(NativeAccountLinkingHandlerTest,
       InitiateAccountLinkingNetworkCall_Success_NotEligible) {
  std::vector<uint8_t> client_token = {1, 2, 3};
  EXPECT_CALL(payments_network_interface_,
              GetDetailsForCreatePaymentInstrument(_, client_token, _, _))
      .WillOnce([](long billing_customer_id, const std::vector<uint8_t>& token,
                   auto callback, const std::string& app_locale) {
        std::move(callback).Run(autofill::payments::PaymentsAutofillClient::
                                    PaymentsRpcResult::kSuccess,
                                /*is_eligible=*/false,
                                /*action_token=*/std::vector<uint8_t>{});
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });
  EXPECT_CALL(*handler_,
              DoOnAccountLinkingResult(AccountLinkingResult{
                  false, 0, AccountLinkingResultCode::kCouldNotInvoke}))
      .Times(1);

  handler_->InitiateAccountLinkingNetworkCall(client_token);

  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.TestFop.AccountLinking."
      "GetDetailsForCreatePaymentInstrument.Result",
      /*sample=*/false, /*expected_bucket_count=*/1);
  histogram_tester_.ExpectTotalCount(
      "FacilitatedPayments.TestFop.AccountLinking."
      "GetDetailsForCreatePaymentInstrument.Latency",
      1);
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.TestFop.AccountLinking.FlowExitedReason",
      /*sample=*/AccountLinkingFlowExitedReason::kNotEligiblePerPaymentsBackend,
      /*expected_bucket_count=*/1);
}

TEST_F(NativeAccountLinkingHandlerTest, OnAccepted_Success) {
  std::vector<uint8_t> client_token = {1, 2, 3};
  std::vector<uint8_t> expected_action_token = {'t', 'o', 'k', 'e', 'n'};
  EXPECT_CALL(payments_network_interface_,
              GetDetailsForCreatePaymentInstrument(_, client_token, _, _))
      .WillOnce([&](long billing_customer_id, const std::vector<uint8_t>& token,
                    auto callback, const std::string& app_locale) {
        std::move(callback).Run(autofill::payments::PaymentsAutofillClient::
                                    PaymentsRpcResult::kSuccess,
                                /*is_eligible=*/true, expected_action_token);
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });

  handler_->InitiateAccountLinkingNetworkCall(client_token);

  CoreAccountInfo account_info =
      payments_data_manager_.GetAccountInfoForPaymentsServer();
  EXPECT_CALL(*api_client_ptr_,
              InvokeInstrumentManager(account_info, expected_action_token, _))
      .WillOnce([&](CoreAccountInfo account, const std::vector<uint8_t>& token,
                    base::OnceCallback<void(AccountLinkingResult)> callback) {
        std::move(callback).Run(AccountLinkingResult{
            true, 12345, AccountLinkingResultCode::kResultOk});
      });
  EXPECT_CALL(*handler_, DoOnAccountLinkingResult(AccountLinkingResult{
                             true, 12345, AccountLinkingResultCode::kResultOk}))
      .Times(1);

  handler_->OnAccepted();

  histogram_tester_.ExpectTotalCount(
      "FacilitatedPayments.TestFop.AccountLinking.FlowExitedReason", 0);
}

TEST_F(NativeAccountLinkingHandlerTest, OnAccepted_InstrumentManagerFails) {
  std::vector<uint8_t> client_token = {1, 2, 3};
  std::vector<uint8_t> expected_action_token = {'t', 'o', 'k', 'e', 'n'};
  EXPECT_CALL(payments_network_interface_,
              GetDetailsForCreatePaymentInstrument(_, client_token, _, _))
      .WillOnce([&](long billing_customer_id, const std::vector<uint8_t>& token,
                    auto callback, const std::string& app_locale) {
        std::move(callback).Run(autofill::payments::PaymentsAutofillClient::
                                    PaymentsRpcResult::kSuccess,
                                /*is_eligible=*/true, expected_action_token);
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });

  handler_->InitiateAccountLinkingNetworkCall(client_token);

  CoreAccountInfo account_info =
      payments_data_manager_.GetAccountInfoForPaymentsServer();
  EXPECT_CALL(*api_client_ptr_,
              InvokeInstrumentManager(account_info, expected_action_token, _))
      .WillOnce([&](CoreAccountInfo account, const std::vector<uint8_t>& token,
                    base::OnceCallback<void(AccountLinkingResult)> callback) {
        std::move(callback).Run(AccountLinkingResult{
            false, 0, AccountLinkingResultCode::kCouldNotInvoke});
      });
  EXPECT_CALL(*handler_,
              DoOnAccountLinkingResult(AccountLinkingResult{
                  false, 0, AccountLinkingResultCode::kCouldNotInvoke}))
      .Times(1);

  handler_->OnAccepted();
}

TEST_F(NativeAccountLinkingHandlerTest, OnAccepted_UserLoggedOut) {
  std::vector<uint8_t> client_token = {1, 2, 3};
  std::vector<uint8_t> expected_action_token = {'t', 'o', 'k', 'e', 'n'};
  EXPECT_CALL(payments_network_interface_,
              GetDetailsForCreatePaymentInstrument(_, client_token, _, _))
      .WillOnce([&](long billing_customer_id, const std::vector<uint8_t>& token,
                    auto callback, const std::string& app_locale) {
        std::move(callback).Run(autofill::payments::PaymentsAutofillClient::
                                    PaymentsRpcResult::kSuccess,
                                /*is_eligible=*/true, expected_action_token);
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });

  handler_->InitiateAccountLinkingNetworkCall(client_token);

  EXPECT_CALL(client_, GetCoreAccountInfo).WillOnce(Return(std::nullopt));
  EXPECT_CALL(*handler_,
              DoOnAccountLinkingResult(AccountLinkingResult{
                  false, 0, AccountLinkingResultCode::kCouldNotInvoke}))
      .Times(1);

  handler_->OnAccepted();

  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.TestFop.AccountLinking.FlowExitedReason",
      /*sample=*/AccountLinkingFlowExitedReason::kUserLoggedOut,
      /*expected_bucket_count=*/1);
}

TEST_F(NativeAccountLinkingHandlerTest, OnAccepted_ApiClientNull) {
  std::vector<uint8_t> client_token = {1, 2, 3};
  std::vector<uint8_t> expected_action_token = {'t', 'o', 'k', 'e', 'n'};
  EXPECT_CALL(payments_network_interface_,
              GetDetailsForCreatePaymentInstrument(_, client_token, _, _))
      .WillOnce([&](long billing_customer_id, const std::vector<uint8_t>& token,
                    auto callback, const std::string& app_locale) {
        std::move(callback).Run(autofill::payments::PaymentsAutofillClient::
                                    PaymentsRpcResult::kSuccess,
                                /*is_eligible=*/true, expected_action_token);
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });

  handler_->InitiateAccountLinkingNetworkCall(client_token);

  // Make the API client creator return null.
  api_client_.reset();

  EXPECT_CALL(*handler_,
              DoOnAccountLinkingResult(AccountLinkingResult{
                  false, 0, AccountLinkingResultCode::kCouldNotInvoke}))
      .Times(1);

  handler_->OnAccepted();

  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.TestFop.AccountLinking.FlowExitedReason",
      /*sample=*/
      AccountLinkingFlowExitedReason::kApiClientNotAvailable,
      /*expected_bucket_count=*/1);
}

TEST_F(NativeAccountLinkingHandlerTest, OnAccepted_NoToken) {
  EXPECT_CALL(*handler_,
              DoOnAccountLinkingResult(AccountLinkingResult{
                  false, 0, AccountLinkingResultCode::kCouldNotInvoke}))
      .Times(1);

  handler_->OnAccepted();

  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.TestFop.AccountLinking.FlowExitedReason",
      /*sample=*/AccountLinkingFlowExitedReason::kActionTokenNotAvailable,
      /*expected_bucket_count=*/1);
}

TEST_F(NativeAccountLinkingHandlerTest, OnDeclined) {
  std::vector<uint8_t> client_token = {1, 2, 3};
  EXPECT_CALL(payments_network_interface_,
              GetDetailsForCreatePaymentInstrument(_, client_token, _, _))
      .WillOnce([](long billing_customer_id, const std::vector<uint8_t>& token,
                   auto callback, const std::string& app_locale) {
        std::move(callback).Run(
            autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
                kSuccess,
            /*is_eligible=*/true,
            /*action_token=*/std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'});
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });

  handler_->InitiateAccountLinkingNetworkCall(client_token);

  EXPECT_CALL(*handler_,
              DoOnAccountLinkingResult(AccountLinkingResult{
                  false, 0, AccountLinkingResultCode::kResultCanceled}))
      .Times(1);

  handler_->OnDeclined();
}

TEST_F(NativeAccountLinkingHandlerTest, ShowAccountLinkingPrompt_Success) {
  base::OnceCallback<void()> on_accepted;
  base::OnceCallback<void()> on_declined;
  base::OnceCallback<void()> on_dismissed;

  AccountLinkingParams params(FacilitatedPaymentsType::kEwallet);

  EXPECT_CALL(*handler_, CreateAccountLinkingParams())
      .WillRepeatedly(Return(params));

  EXPECT_CALL(client_,
              ShowAccountLinkingPrompt(testing::AllOf(testing::Field(
                                           &AccountLinkingParams::fop_type,
                                           FacilitatedPaymentsType::kEwallet)),
                                       _, _, _))
      .WillRepeatedly([&](const AccountLinkingParams& p,
                          base::OnceCallback<void()> accepted,
                          base::OnceCallback<void()> declined,
                          base::OnceCallback<void()> dismissed) {
        on_accepted = std::move(accepted);
        on_declined = std::move(declined);
        on_dismissed = std::move(dismissed);
      });

  // Base class OnAccepted/OnDeclined/Dismiss all trigger client_.DismissPrompt.
  EXPECT_CALL(client_, DismissPrompt()).Times(3);

  EXPECT_CALL(*handler_,
              DoOnAccountLinkingResult(AccountLinkingResult{
                  false, 0, AccountLinkingResultCode::kResultCanceled}))
      .Times(2);

  EXPECT_CALL(*handler_, DoOnAccountLinkingResult(AccountLinkingResult{}))
      .Times(1);

  handler_->ShowAccountLinkingPrompt();
  EXPECT_TRUE(handler_->is_prompt_showing());
  std::move(on_accepted).Run();
  EXPECT_FALSE(handler_->is_prompt_showing());

  handler_->ShowAccountLinkingPrompt();
  EXPECT_TRUE(handler_->is_prompt_showing());
  std::move(on_declined).Run();
  EXPECT_FALSE(handler_->is_prompt_showing());

  handler_->ShowAccountLinkingPrompt();
  EXPECT_TRUE(handler_->is_prompt_showing());
  std::move(on_dismissed).Run();
  EXPECT_FALSE(handler_->is_prompt_showing());
}

TEST_F(NativeAccountLinkingHandlerTest, ShowAccountLinkingPrompt_FopNullopt) {
  EXPECT_CALL(*handler_, CreateAccountLinkingParams())
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(client_, ShowAccountLinkingPrompt(_, _, _, _)).Times(0);
  handler_->ShowAccountLinkingPrompt();
  EXPECT_FALSE(handler_->is_prompt_showing());
}

TEST_F(NativeAccountLinkingHandlerTest,
       CanPromptUser_MaxStrikesReached_ReturnsFalseAndLogsHistogram) {
  test_strike_database_->AddStrike();
  test_strike_database_->AddStrike();
  test_strike_database_->AddStrike();
  handler_->set_is_user_pref_enabled(true);
  EXPECT_CALL(client_, HasScreenlockOrBiometricSetup())
      .WillRepeatedly(Return(true));

  EXPECT_FALSE(handler_->CanPromptUser());
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.TestFop.AccountLinking.FlowExitedReason",
      AccountLinkingFlowExitedReason::kMaxStrikes, 1);
}

TEST_F(NativeAccountLinkingHandlerTest,
       CanPromptUser_RequiredDelayNotPassed_ReturnsFalseAndLogsHistogram) {
  test_strike_database_->AddStrike();
  handler_->set_is_user_pref_enabled(true);
  EXPECT_CALL(client_, HasScreenlockOrBiometricSetup())
      .WillRepeatedly(Return(true));

  EXPECT_FALSE(handler_->CanPromptUser());
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.TestFop.AccountLinking.FlowExitedReason",
      AccountLinkingFlowExitedReason::kRequiredDelayNotPassed, 1);

  task_environment_.FastForwardBy(base::Days(6) + base::Hours(23));
  EXPECT_FALSE(handler_->CanPromptUser());

  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_TRUE(handler_->CanPromptUser());
}

TEST_F(NativeAccountLinkingHandlerTest,
       CanPromptUser_UserPrefDisabled_ReturnsFalseAndLogsHistogram) {
  handler_->set_is_user_pref_enabled(false);
  EXPECT_FALSE(handler_->CanPromptUser());
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.TestFop.AccountLinking.FlowExitedReason",
      AccountLinkingFlowExitedReason::kUserOptedOut, 1);
}

TEST_F(NativeAccountLinkingHandlerTest,
       CanPromptUser_NoScreenlockOrBiometrics_ReturnsFalseAndLogsHistogram) {
  handler_->set_is_user_pref_enabled(true);
  EXPECT_CALL(client_, HasScreenlockOrBiometricSetup())
      .WillOnce(Return(false));
  EXPECT_FALSE(handler_->CanPromptUser());
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.TestFop.AccountLinking.FlowExitedReason",
      AccountLinkingFlowExitedReason::kNoScreenlockOrBiometricSetup, 1);
}

TEST_F(NativeAccountLinkingHandlerTest, CanPromptUser_AllConditionsMet_ReturnsTrue) {
  handler_->set_is_user_pref_enabled(true);
  EXPECT_CALL(client_, HasScreenlockOrBiometricSetup())
      .WillOnce(Return(true));
  EXPECT_TRUE(handler_->CanPromptUser());
  histogram_tester_.ExpectTotalCount(
      "FacilitatedPayments.TestFop.AccountLinking.FlowExitedReason", 0);
}

TEST_F(NativeAccountLinkingHandlerTest,
       CanPromptUser_StrictCheckOrder_StrikeLimitTakesPrecedence) {
  test_strike_database_->AddStrike();
  test_strike_database_->AddStrike();
  test_strike_database_->AddStrike();
  handler_->set_is_user_pref_enabled(false);
  EXPECT_CALL(client_, HasScreenlockOrBiometricSetup())
      .WillRepeatedly(Return(false));

  EXPECT_FALSE(handler_->CanPromptUser());
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.TestFop.AccountLinking.FlowExitedReason",
      AccountLinkingFlowExitedReason::kMaxStrikes, 1);
}

TEST_F(NativeAccountLinkingHandlerTest,
       CanPromptUser_IncognitoNullStrikeDatabase_ReturnsTrue) {
  handler_->set_strike_database(nullptr);
  handler_->set_is_user_pref_enabled(true);
  EXPECT_CALL(client_, HasScreenlockOrBiometricSetup())
      .WillOnce(Return(true));
  EXPECT_TRUE(handler_->CanPromptUser());
  histogram_tester_.ExpectTotalCount(
      "FacilitatedPayments.TestFop.AccountLinking.FlowExitedReason", 0);
}

TEST_F(NativeAccountLinkingHandlerTest,
       OnAccepted_IncognitoNullStrikeDatabase_DoesNotCrash) {
  handler_->set_strike_database(nullptr);
  handler_->OnAccepted();
}

TEST_F(NativeAccountLinkingHandlerTest,
       OnDeclined_IncognitoNullStrikeDatabase_DoesNotCrash) {
  handler_->set_strike_database(nullptr);
  handler_->OnDeclined();
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.TestFop.AccountLinking.FlowExitedReason",
      AccountLinkingFlowExitedReason::kUserDeclined, 1);
}

TEST_F(NativeAccountLinkingHandlerTest, OnAccepted_ClearsStrikesInDatabase) {
  test_strike_database_->AddStrike();
  test_strike_database_->AddStrike();
  ASSERT_EQ(test_strike_database_->GetStrikes(), 2);

  handler_->OnAccepted();
  EXPECT_EQ(test_strike_database_->GetStrikes(), 0);
}

TEST_F(NativeAccountLinkingHandlerTest, OnDeclined_RecordsStrikeInDatabase) {
  ASSERT_EQ(test_strike_database_->GetStrikes(), 0);

  handler_->OnDeclined();
  EXPECT_EQ(test_strike_database_->GetStrikes(), 1);
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.TestFop.AccountLinking.FlowExitedReason",
      AccountLinkingFlowExitedReason::kUserDeclined, 1);
}

TEST_F(NativeAccountLinkingHandlerTest, OnDismissed_DoesNotRecordStrike) {
  ASSERT_EQ(test_strike_database_->GetStrikes(), 0);

  handler_->OnDismissed();
  EXPECT_EQ(test_strike_database_->GetStrikes(), 0);
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.TestFop.AccountLinking.FlowExitedReason",
      AccountLinkingFlowExitedReason::kScreenClosedByUser, 1);
}

}  // namespace
}  // namespace payments::facilitated
