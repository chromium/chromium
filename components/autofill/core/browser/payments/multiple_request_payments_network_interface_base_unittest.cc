// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/multiple_request_payments_network_interface_base.h"

#include <memory>

#include "components/autofill/core/browser/payments/payments_network_interface_base.h"
#include "components/autofill/core/browser/payments/payments_network_interface_test_base.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/net_errors.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

namespace {
using RequestOperation =
    MultipleRequestPaymentsNetworkInterfaceBase::RequestOperation;
}  // namespace

class MockPaymentsRequest : public PaymentsRequest {
 public:
  MockPaymentsRequest() = default;
  ~MockPaymentsRequest() override = default;

  MOCK_METHOD(std::string, GetRequestUrlPath, (), (override));
  MOCK_METHOD(std::string, GetRequestContentType, (), (override));
  MOCK_METHOD(std::string, GetRequestContent, (), (override));
  MOCK_METHOD(void, ParseResponse, (const base::Value::Dict&), (override));
  MOCK_METHOD(bool, IsResponseComplete, (), (override));
  MOCK_METHOD(void,
              RespondToDelegate,
              (PaymentsAutofillClient::PaymentsRpcResult),
              (override));
};

// Tests to make sure multiple requests at one time are supported.
class MultipleRequestsTest : public PaymentsNetworkInterfaceTestBase,
                             public testing::Test {
 public:
  void SetUp() override {
    SetUpTest();
    payments_network_interface_base_ =
        std::make_unique<MultipleRequestPaymentsNetworkInterfaceBase>(
            test_shared_loader_factory_, *identity_test_env_.identity_manager(),
            /*is_off_the_record=*/false);
    request1_ = std::make_unique<testing::NiceMock<MockPaymentsRequest>>();
    EXPECT_CALL(*request1_, IsResponseComplete)
        .WillRepeatedly(testing::Return(true));
    request2_ = std::make_unique<testing::NiceMock<MockPaymentsRequest>>();
    EXPECT_CALL(*request2_, IsResponseComplete)
        .WillRepeatedly(testing::Return(true));
  }

 protected:
  size_t operations_size() const {
    return payments_network_interface_base_->operations_for_testing().size();
  }

  // TODO: crbug.com/362787977 - After single request PaymentsNetworkInterface
  // is cleaned up, move this function to the
  // payments_network_interface_test_base.*.
  void ReturnResponseForOperation(const RequestId& id,
                                  int response_code,
                                  const std::string& response_body) {
    EXPECT_TRUE(
        payments_network_interface_base_->operations_for_testing().contains(
            id));
    payments_network_interface_base_->operations_for_testing()
        .at(id)
        ->OnSimpleLoaderCompleteInternalForTesting(response_code,
                                                   response_body);
  }

  std::unique_ptr<MultipleRequestPaymentsNetworkInterfaceBase>
      payments_network_interface_base_;
  std::unique_ptr<testing::NiceMock<MockPaymentsRequest>> request1_;
  std::unique_ptr<testing::NiceMock<MockPaymentsRequest>> request2_;
};

// Tests that two requests both succeeded and they work independently.
TEST_F(MultipleRequestsTest, Success) {
  EXPECT_CALL(*request1_, RespondToDelegate(PaymentsRpcResult::kSuccess));
  EXPECT_CALL(*request2_, RespondToDelegate(PaymentsRpcResult::kSuccess));

  // First request starts.
  RequestId id1 =
      payments_network_interface_base_->IssueRequest(std::move(request1_));
  IssueOAuthToken();
  EXPECT_EQ(1U, operations_size());

  // Second request starts.
  RequestId id2 =
      payments_network_interface_base_->IssueRequest(std::move(request2_));
  IssueOAuthToken();
  EXPECT_EQ(2U, operations_size());

  // Simulate response for first request is received.
  ReturnResponseForOperation(id1, net::HTTP_OK, "{}");
  EXPECT_EQ(1U, operations_size());

  // Response for second request is received.
  ReturnResponseForOperation(id2, net::HTTP_OK, "{}");
  EXPECT_EQ(0U, operations_size());
}

// Tests that one request failing in access token fetching does not affect
// another request.
TEST_F(MultipleRequestsTest, AccessTokenFetchFailed) {
  EXPECT_CALL(*request1_, RespondToDelegate(PaymentsRpcResult::kSuccess));
  EXPECT_CALL(*request2_,
              RespondToDelegate(PaymentsRpcResult::kPermanentFailure));

  // First request starts.
  RequestId id1 =
      payments_network_interface_base_->IssueRequest(std::move(request1_));
  IssueOAuthToken();
  EXPECT_EQ(1U, operations_size());

  // Second request is never sent because of the access token fetching failure.
  payments_network_interface_base_->IssueRequest(std::move(request2_));
  EXPECT_EQ(2U, operations_size());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_TIMED_OUT));
  EXPECT_EQ(1U, operations_size());

  // Simulate response for first request is received.
  ReturnResponseForOperation(id1, net::HTTP_OK, "{}");
  EXPECT_EQ(0U, operations_size());
}

// Tests that one request having access token expired does not affect
// another request. The request should refetch the access token and assume
// resent request succeeded.
TEST_F(MultipleRequestsTest, AccessTokenExpired_RetrySucceeded) {
  EXPECT_CALL(*request1_, RespondToDelegate(PaymentsRpcResult::kSuccess));
  EXPECT_CALL(*request2_, RespondToDelegate(PaymentsRpcResult::kSuccess));

  // First request starts.
  RequestId id1 =
      payments_network_interface_base_->IssueRequest(std::move(request1_));
  IssueOAuthToken();
  EXPECT_EQ(1U, operations_size());

  // Second request starts.
  RequestId id2 =
      payments_network_interface_base_->IssueRequest(std::move(request2_));
  EXPECT_EQ(2U, operations_size());

  // Simulate response for first request is received, but with an unauthorized
  // error. Token refetch should start.
  ReturnResponseForOperation(id1, net::HTTP_UNAUTHORIZED, "{}");
  EXPECT_EQ(2U, operations_size());

  // Issue another token and resend request.
  IssueOAuthToken();
  EXPECT_EQ(2U, operations_size());
  ReturnResponseForOperation(id1, net::HTTP_OK, "{}");
  EXPECT_EQ(1U, operations_size());

  // Response for the second request is received.
  ReturnResponseForOperation(id2, net::HTTP_OK, "{}");
  EXPECT_EQ(0U, operations_size());
}

// Tests that one request having access token expired does not affect
// another request. The request should refetch the access token and assume
// resent request still failed with unauthorized error.
TEST_F(MultipleRequestsTest, AccessTokenExpired_RetryFailed) {
  EXPECT_CALL(*request1_,
              RespondToDelegate(PaymentsRpcResult::kPermanentFailure));
  EXPECT_CALL(*request2_, RespondToDelegate(PaymentsRpcResult::kSuccess));

  // First request starts.
  RequestId id1 =
      payments_network_interface_base_->IssueRequest(std::move(request1_));
  IssueOAuthToken();
  EXPECT_EQ(1U, operations_size());

  // Second request starts.
  RequestId id2 =
      payments_network_interface_base_->IssueRequest(std::move(request2_));
  EXPECT_EQ(2U, operations_size());

  // Simulate response for first request is received, but with an unauthorized
  // error. Token refetch should start.
  ReturnResponseForOperation(id1, net::HTTP_UNAUTHORIZED, "{}");
  EXPECT_EQ(2U, operations_size());

  // Issue another token and resend request, but still receive an unauthorized
  // error. This time the request will end with permanent failure.
  IssueOAuthToken();
  EXPECT_EQ(2U, operations_size());
  ReturnResponseForOperation(id1, net::HTTP_UNAUTHORIZED, "{}");
  EXPECT_EQ(1U, operations_size());

  // Response for the second request is received.
  ReturnResponseForOperation(id2, net::HTTP_OK, "{}");
  EXPECT_EQ(0U, operations_size());
}

// Test that one request with an error response does not affect another request.
TEST_F(MultipleRequestsTest, ResponseError) {
  EXPECT_CALL(*request1_,
              RespondToDelegate(PaymentsRpcResult::kTryAgainFailure));
  EXPECT_CALL(*request2_, RespondToDelegate(PaymentsRpcResult::kSuccess));

  // First request starts.
  RequestId id1 =
      payments_network_interface_base_->IssueRequest(std::move(request1_));
  IssueOAuthToken();
  EXPECT_EQ(1U, operations_size());

  // Second request starts.
  RequestId id2 =
      payments_network_interface_base_->IssueRequest(std::move(request2_));
  IssueOAuthToken();
  EXPECT_EQ(2U, operations_size());

  // Simulate response for first request is received but with a temporary error.
  ReturnResponseForOperation(id1, net::HTTP_OK,
                             "{\"error\": {\"code\": \"internal\", "
                             "\"api_error_reason\": \"\"}, "
                             "\"decline_details\": {\"user_message_title\": "
                             "\"\", \"user_message_description\": "
                             "\"\"}}");
  EXPECT_EQ(1U, operations_size());

  // Response for second request is received.
  ReturnResponseForOperation(id2, net::HTTP_OK, "{}");
  EXPECT_EQ(0U, operations_size());
}

// Tests that canceling one request does not affect other requests.
TEST_F(MultipleRequestsTest, RequestCanceled) {
  EXPECT_CALL(*request1_, RespondToDelegate(PaymentsRpcResult::kSuccess));
  EXPECT_CALL(*request2_, RespondToDelegate).Times(0);

  // First request starts.
  RequestId id1 =
      payments_network_interface_base_->IssueRequest(std::move(request1_));
  IssueOAuthToken();
  EXPECT_EQ(1U, operations_size());

  // Second request starts.
  RequestId id2 =
      payments_network_interface_base_->IssueRequest(std::move(request2_));
  IssueOAuthToken();
  EXPECT_EQ(2U, operations_size());

  // Cancel the second request (it is invalidated but is still being tracked).
  payments_network_interface_base_->CancelRequestWithId(id2);
  EXPECT_EQ(2U, operations_size());

  // Simulate response for first request is received.
  ReturnResponseForOperation(id1, net::HTTP_OK, "{}");
  EXPECT_EQ(1U, operations_size());
  // Simulate response for second request is received.
  ReturnResponseForOperation(id2, net::HTTP_OK, "{}");
  EXPECT_EQ(0U, operations_size());
}

}  // namespace autofill::payments
