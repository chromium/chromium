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
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/mock_facilitated_payments_network_interface.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

using ::testing::_;
using ::testing::Return;

class TestNativeAccountLinkingHandler : public NativeAccountLinkingHandler {
 public:
  using NativeAccountLinkingHandler::InitiateAccountLinkingNetworkCall;

  TestNativeAccountLinkingHandler(
      FacilitatedPaymentsClient* client,
      FacilitatedPaymentsApiClientCreator api_client_creator)
      : NativeAccountLinkingHandler(client, std::move(api_client_creator)) {}
  ~TestNativeAccountLinkingHandler() override = default;

  MOCK_METHOD(void,
              DoOnClientTokenReceived,
              (const std::vector<uint8_t>& client_token),
              (override));
  MOCK_METHOD(void, DoOnAccountLinkingResult, (bool success), (override));

  std::string_view GetHistogramSuffix() const override { return "TestFop"; }

  base::DictValue GetPayloadForGetDetailsForCreatePaymentInstrument() override {
    base::DictValue payload;
    payload.Set("test_key", "test_value");
    return payload;
  }
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

    api_client_ = std::make_unique<MockFacilitatedPaymentsApiClient>();
    api_client_ptr_ = api_client_.get();

    handler_ = std::make_unique<TestNativeAccountLinkingHandler>(
        &client_,
        base::BindRepeating(&NativeAccountLinkingHandlerTest::CreateApiClient,
                            base::Unretained(this)));
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
  EXPECT_CALL(*handler_, DoOnAccountLinkingResult(false)).Times(1);

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

TEST_F(NativeAccountLinkingHandlerTest, InitiateNetworkCall_Success) {
  std::vector<uint8_t> client_token = {1, 2, 3};
  EXPECT_CALL(payments_network_interface_,
              GetDetailsForCreatePaymentInstrument(_, _, _))
      .WillOnce([](long billing_customer_id, auto callback,
                   const std::string& app_locale) {
        std::move(callback).Run(autofill::payments::PaymentsAutofillClient::
                                    PaymentsRpcResult::kSuccess,
                                /*is_eligible=*/true);
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });

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

TEST_F(NativeAccountLinkingHandlerTest, InitiateNetworkCall_Failure) {
  std::vector<uint8_t> client_token = {1, 2, 3};
  EXPECT_CALL(payments_network_interface_,
              GetDetailsForCreatePaymentInstrument(_, _, _))
      .WillOnce([](long billing_customer_id, auto callback,
                   const std::string& app_locale) {
        std::move(callback).Run(autofill::payments::PaymentsAutofillClient::
                                    PaymentsRpcResult::kPermanentFailure,
                                /*is_eligible=*/false);
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });
  EXPECT_CALL(*handler_, DoOnAccountLinkingResult(false)).Times(1);

  handler_->InitiateAccountLinkingNetworkCall(client_token);

  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.TestFop.AccountLinking."
      "GetDetailsForCreatePaymentInstrument.Result",
      /*sample=*/false, /*expected_bucket_count=*/1);
  histogram_tester_.ExpectTotalCount(
      "FacilitatedPayments.TestFop.AccountLinking."
      "GetDetailsForCreatePaymentInstrument.Latency",
      1);
}

}  // namespace
}  // namespace payments::facilitated
