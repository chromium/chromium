// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_request.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::boca {
namespace {

constexpr std::string_view kRelativeUrl = "test/path";
constexpr std::string_view kFullUrl =
    "https://schooltools-pa.googleapis.com/test/path";
constexpr std::string_view kRequestBody = R"({"request_key": "request_value"})";
constexpr std::string_view kResponsekey = "response_key";
constexpr std::string_view kResponseValue = "response_value";
constexpr std::string_view kResponseBody =
    R"({"response_key": "response_value"})";

class MockBocaRequestDelegate : public BocaRequest::Delegate {
 public:
  MockBocaRequestDelegate() = default;
  ~MockBocaRequestDelegate() override = default;

  MOCK_METHOD(std::string, GetRelativeUrl, (), (override));
  MOCK_METHOD(std::optional<std::string>, GetRequestBody, (), (override));
  MOCK_METHOD(void,
              OnSuccess,
              (std::unique_ptr<base::Value> response),
              (override));
  MOCK_METHOD(void, OnError, (google_apis::ApiErrorCode error), (override));
  MOCK_METHOD(google_apis::HttpRequestMethod,
              GetRequestType,
              (),
              (const, override));
};

class BocaRequestTest : public testing::Test {
 protected:
  void SetUp() override {
    auto auth_service = std::make_unique<google_apis::DummyAuthService>();
    auth_service_ptr_ = auth_service.get();
    sender_ = std::make_unique<google_apis::RequestSender>(
        std::move(auth_service), url_loader_factory_.GetSafeWeakWrapper(),
        task_environment_.GetMainThreadTaskRunner(), "custom-user-agent",
        TRAFFIC_ANNOTATION_FOR_TESTS);
    delegate_ = std::make_unique<testing::NiceMock<MockBocaRequestDelegate>>();
    ON_CALL(*delegate_, GetRequestType)
        .WillByDefault(testing::Return(google_apis::HttpRequestMethod::kPost));
    EXPECT_CALL(*delegate_, GetRelativeUrl)
        .WillOnce(testing::Return(std::string(kRelativeUrl)));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory url_loader_factory_;
  std::unique_ptr<google_apis::RequestSender> sender_;
  std::unique_ptr<testing::NiceMock<MockBocaRequestDelegate>> delegate_;
  raw_ptr<google_apis::DummyAuthService> auth_service_ptr_;
};

TEST_F(BocaRequestTest, RequestDataAreCorrect) {
  EXPECT_CALL(*delegate_, GetRequestBody)
      .WillOnce(testing::Return(std::string(kRequestBody)));

  auto boca_request =
      std::make_unique<BocaRequest>(sender_.get(), std::move(delegate_));
  sender_->StartRequestWithAuthRetry(std::move(boca_request));
  url_loader_factory_.WaitForRequest(GURL(kFullUrl));
  network::ResourceRequest request =
      url_loader_factory_.GetPendingRequest(0)->request;
  std::string_view actual_request_body = request.request_body->elements()
                                             ->at(0)
                                             .As<network::DataElementBytes>()
                                             .AsStringPiece();

  EXPECT_THAT(actual_request_body, testing::StrEq(kRequestBody));
  EXPECT_EQ(request.method, net::HttpRequestHeaders::kPostMethod);
}

TEST_F(BocaRequestTest, EmptyRequest) {
  ON_CALL(*delegate_, GetRequestType)
      .WillByDefault(testing::Return(google_apis::HttpRequestMethod::kGet));
  EXPECT_CALL(*delegate_, GetRequestBody)
      .WillOnce(testing::Return(std::nullopt));

  auto boca_request =
      std::make_unique<BocaRequest>(sender_.get(), std::move(delegate_));
  sender_->StartRequestWithAuthRetry(std::move(boca_request));
  url_loader_factory_.WaitForRequest(GURL(kFullUrl));
  network::ResourceRequest request =
      url_loader_factory_.GetPendingRequest(0)->request;

  EXPECT_THAT(request.request_body, testing::IsNull());
  EXPECT_EQ(request.method, net::HttpRequestHeaders::kGetMethod);
}

TEST_F(BocaRequestTest, SuccessfullRequest) {
  base::test::TestFuture<std::unique_ptr<base::Value>> future;
  EXPECT_CALL(*delegate_, GetRequestBody)
      .WillOnce(testing::Return(std::string(kRequestBody)));
  EXPECT_CALL(*delegate_, OnSuccess)
      .WillOnce([&future](std::unique_ptr<base::Value> value) {
        std::move(future).GetCallback().Run(std::move(value));
      });
  EXPECT_CALL(*delegate_, OnError).Times(0);

  auto boca_request =
      std::make_unique<BocaRequest>(sender_.get(), std::move(delegate_));
  url_loader_factory_.AddResponse(std::string(kFullUrl),
                                  std::string(kResponseBody));
  sender_->StartRequestWithAuthRetry(std::move(boca_request));
  std::unique_ptr<base::Value> response = future.Take();

  ASSERT_THAT(response, testing::NotNull());
  ASSERT_TRUE(response->is_dict());
  EXPECT_THAT(*response->GetDict().FindString(kResponsekey),
              testing::StrEq(kResponseValue));
}

TEST_F(BocaRequestTest, FailedRequest) {
  base::test::TestFuture<google_apis::ApiErrorCode> future;
  EXPECT_CALL(*delegate_, GetRequestBody)
      .WillOnce(testing::Return(std::string(kRequestBody)));
  EXPECT_CALL(*delegate_, OnSuccess).Times(0);
  EXPECT_CALL(*delegate_, OnError)
      .WillOnce([&future](google_apis::ApiErrorCode error) {
        std::move(future).GetCallback().Run(error);
      });

  auto boca_request =
      std::make_unique<BocaRequest>(sender_.get(), std::move(delegate_));
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_head->headers->ReplaceStatusLine("HTTP/1.1 404 Not found");
  url_loader_factory_.AddResponse(GURL(kFullUrl), std::move(response_head),
                                  /*content=*/"",
                                  network::URLLoaderCompletionStatus(net::OK));
  sender_->StartRequestWithAuthRetry(std::move(boca_request));

  EXPECT_EQ(future.Get(), google_apis::ApiErrorCode::HTTP_NOT_FOUND);
}

TEST_F(BocaRequestTest, ParseError) {
  base::test::TestFuture<google_apis::ApiErrorCode> future;
  EXPECT_CALL(*delegate_, GetRequestBody)
      .WillOnce(testing::Return(std::string(kRequestBody)));
  EXPECT_CALL(*delegate_, OnSuccess).Times(0);
  EXPECT_CALL(*delegate_, OnError)
      .WillOnce([&future](google_apis::ApiErrorCode error) {
        std::move(future).GetCallback().Run(error);
      });

  auto boca_request =
      std::make_unique<BocaRequest>(sender_.get(), std::move(delegate_));
  url_loader_factory_.AddResponse(std::string(kFullUrl), "invalid json");
  sender_->StartRequestWithAuthRetry(std::move(boca_request));

  EXPECT_EQ(future.Get(), google_apis::ApiErrorCode::PARSE_ERROR);
}

TEST_F(BocaRequestTest, EmptyResponse) {
  base::test::TestFuture<std::unique_ptr<base::Value>> future;
  EXPECT_CALL(*delegate_, GetRequestBody)
      .WillOnce(testing::Return(std::string(kRequestBody)));
  EXPECT_CALL(*delegate_, OnSuccess)
      .WillOnce([&future](std::unique_ptr<base::Value> value) {
        std::move(future).GetCallback().Run(std::move(value));
      });
  EXPECT_CALL(*delegate_, OnError).Times(0);

  auto boca_request =
      std::make_unique<BocaRequest>(sender_.get(), std::move(delegate_));
  url_loader_factory_.AddResponse(std::string(kFullUrl), "{}");
  sender_->StartRequestWithAuthRetry(std::move(boca_request));
  std::unique_ptr<base::Value> response = future.Take();

  ASSERT_THAT(response, testing::NotNull());
  ASSERT_TRUE(response->is_dict());
  EXPECT_TRUE(response->GetIfDict()->empty());
}

}  // namespace
}  // namespace ash::boca
