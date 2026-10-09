// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_data_for_agent_request.h"

#include <memory>
#include <utility>

#include "base/test/mock_callback.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

using ::base::MockCallback;
using ::testing::_;
using ::testing::SaveArg;
using Dict = ::base::DictValue;
using PaymentsRpcResult = PaymentsAutofillClient::PaymentsRpcResult;

// The JSON string that the server returns in `payment_token`.
constexpr char kPaymentToken[] =
    R"({"paymentMethod":"CARD","paymentMethodDetails":{)"
    R"("pan":"4111111111111111","cvc":"123",)"
    R"("expirationMonth":12,"expirationYear":2030,"authMethod":"PAN_ONLY"}})";

// The JSON string that the server returns in `mock_payment_token`.
constexpr char kMockPaymentToken[] =
    R"({"paymentMethod":"CARD","paymentMethodDetails":{)"
    R"("pan":"371449635398431","cvc":"2817",)"
    R"("expirationMonth":12,"expirationYear":2030,"authMethod":"PAN_ONLY"}})";

class GetDataForAgentRequestTest : public testing::Test {
 public:
  void SetUp() override {
    GetDataForAgentRequestDetails request_details;
    request_details.opaque_token = "OPAQUE_TOKEN";
    request_details.app_locale = "en-US";
    request_ = std::make_unique<GetDataForAgentRequest>(
        std::move(request_details), mock_callback_.Get());
  }

 protected:
  // Responds to the delegate with `kSuccess` and returns the details that were
  // passed to the callback.
  GetDataForAgentResponseDetails RespondAndGetDetails() {
    GetDataForAgentResponseDetails details;
    EXPECT_CALL(mock_callback_, Run(PaymentsRpcResult::kSuccess, _))
        .WillOnce(SaveArg<1>(&details));
    request_->RespondToDelegate(PaymentsRpcResult::kSuccess);
    return details;
  }

  MockCallback<GetDataForAgentRequest::GetDataForAgentCallback> mock_callback_;
  std::unique_ptr<GetDataForAgentRequest> request_;
};

TEST_F(GetDataForAgentRequestTest, GetRequestUrlPath) {
  EXPECT_EQ(request_->GetRequestUrlPath(),
            "payments/apis-secure/chromepaymentsservice/getdataforagent");
}

TEST_F(GetDataForAgentRequestTest, GetRequestContentType) {
  EXPECT_EQ(request_->GetRequestContentType(), "application/json");
}

TEST_F(GetDataForAgentRequestTest, GetRequestContent) {
  EXPECT_EQ(
      base::test::ParseJsonDict(request_->GetRequestContent()),
      Dict()
          .Set("encrypted_token", "OPAQUE_TOKEN")
          .Set("context", Dict()
                              .Set("language_code", "en-US")
                              .Set("billable_service",
                                   kUnmaskPaymentMethodBillableServiceNumber)));
}

TEST_F(GetDataForAgentRequestTest, IsResponseComplete_ParseResponseNotCalled) {
  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetDataForAgentRequestTest, ParsesPaymentToken) {
  request_->ParseResponse(Dict()
                              .Set("payment_token", kPaymentToken)
                              .Set("mock_payment_token", kMockPaymentToken));
  EXPECT_TRUE(request_->IsResponseComplete());

  GetDataForAgentResponseDetails details = RespondAndGetDetails();
  EXPECT_EQ(details.card_number, "4111111111111111");
  EXPECT_EQ(details.cvc, "123");
  EXPECT_EQ(details.expiration_month, 12);
  EXPECT_EQ(details.expiration_year, 2030);
}

// The test card in `mock_payment_token` is never used, even if
// `payment_token` cannot be parsed.
TEST_F(GetDataForAgentRequestTest, IgnoresMockPaymentToken) {
  request_->ParseResponse(
      Dict()
          .Set("payment_token", "0c6f2a52-7b1e-4c55-9d0b-5c7f0f6b3b0e")
          .Set("mock_payment_token", kMockPaymentToken));
  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetDataForAgentRequestTest, IgnoresAgenticPaymentCredentials) {
  request_->ParseResponse(Dict().Set("agentic_payment_credentials",
                                     Dict()
                                         .Set("token", "5555555555554444")
                                         .Set("cvv", "456")
                                         .Set("expiration_month", 7)
                                         .Set("expiration_year", 2031)));
  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetDataForAgentRequestTest, IncompleteWithoutExpirationYear) {
  request_->ParseResponse(Dict().Set(
      "payment_token",
      R"({"paymentMethodDetails":{"pan":"4111111111111111","cvc":"123",)"
      R"("expirationMonth":12}})"));
  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetDataForAgentRequestTest, IncompleteWithInvalidExpirationMonth) {
  request_->ParseResponse(Dict().Set(
      "payment_token",
      R"({"paymentMethodDetails":{"pan":"4111111111111111","cvc":"123",)"
      R"("expirationMonth":13,"expirationYear":2030}})"));
  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetDataForAgentRequestTest, AcceptsMissingCvc) {
  request_->ParseResponse(Dict().Set(
      "payment_token", R"({"paymentMethodDetails":{"pan":"4111111111111111",)"
                       R"("expirationMonth":12,"expirationYear":2030}})"));
  EXPECT_TRUE(request_->IsResponseComplete());

  GetDataForAgentResponseDetails details = RespondAndGetDetails();
  EXPECT_TRUE(details.cvc.empty());
}

TEST_F(GetDataForAgentRequestTest, RespondToDelegatePassesResult) {
  EXPECT_CALL(mock_callback_, Run(PaymentsRpcResult::kPermanentFailure, _));
  request_->RespondToDelegate(PaymentsRpcResult::kPermanentFailure);
}

}  // namespace
}  // namespace autofill::payments
