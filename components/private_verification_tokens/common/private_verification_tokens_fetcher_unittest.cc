// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_fetcher.h"

#include <memory>
#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace private_verification_tokens {

namespace {

constexpr char kExpectedAcceptHeaderValue[] =
    "application/private-token-response";
constexpr char kExpectedContentTypeHeaderValue[] =
    "application/private-token-request";
constexpr base::TimeDelta kFetchTimeout = base::Minutes(1);
constexpr size_t kResponseMaxBodySize = 2 * 1024;
constexpr char kIssuerServerUrl[] = "http://main.example:8080/issuepvt";

class PrivateVerificationTokensFetcherTest : public testing::Test {
 protected:
  void SetUp() override {
    pvt_server_issue_url_ = GURL(kIssuerServerUrl);
    fetcher_ = PrivateVerificationTokensFetcher::Create(
        pvt_server_issue_url_,
        test_url_loader_factory_.GetSafeWeakWrapper()->Clone());
    ASSERT_TRUE(fetcher_);
  }

 public:
  void SetResponse(std::string response,
                   std::string expected_request_body,
                   base::TimeDelta response_delay = base::Seconds(0)) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [this, response, expected_request_body,
         response_delay](const network::ResourceRequest& request) {
          EXPECT_TRUE(request.url.is_valid());
          EXPECT_EQ(request.url, pvt_server_issue_url_);
          EXPECT_EQ(request.method, net::HttpRequestHeaders::kPostMethod);
          EXPECT_EQ(request.credentials_mode,
                    network::mojom::CredentialsMode::kInclude);
          EXPECT_THAT(
              request.headers.GetHeader(net::HttpRequestHeaders::kAccept),
              testing::Optional(std::string(kExpectedAcceptHeaderValue)));
          EXPECT_THAT(
              request.headers.GetHeader(net::HttpRequestHeaders::kContentType),
              testing::Optional(std::string(kExpectedContentTypeHeaderValue)));
          ASSERT_TRUE(request.request_body);
          const std::string request_body = network::GetUploadData(request);
          EXPECT_EQ(request_body, expected_request_body);
          task_environment_.FastForwardBy(response_delay);
          auto head = network::mojom::URLResponseHead::New();
          test_url_loader_factory_.AddResponse(
              pvt_server_issue_url_, std::move(head), response,
              network::URLLoaderCompletionStatus(net::OK));
        }));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  GURL pvt_server_issue_url_;
  std::unique_ptr<PrivateVerificationTokensFetcher> fetcher_;
};

TEST_F(PrivateVerificationTokensFetcherTest, TryGetTokensSuccess) {
  const std::string request_body = "token-request-bytes";
  const std::string response_body = "token-response-bytes";
  SetResponse(response_body, /*expected_request_body = */ request_body);

  base::test::TestFuture<base::expected<std::string, TryGetTokensResult>>
      future;
  fetcher_->TryGetTokens(request_body, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get<0>();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result, response_body);
}

TEST_F(PrivateVerificationTokensFetcherTest, EmptyResponse) {
  const std::string request_body = "token-request-bytes";
  const std::string response_body = "";
  SetResponse(response_body, /*expected_request_body = */ request_body);

  base::test::TestFuture<base::expected<std::string, TryGetTokensResult>>
      future;
  fetcher_->TryGetTokens(request_body, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get<0>();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result, response_body);
}

TEST_F(PrivateVerificationTokensFetcherTest, LargeResponseWithinLimit) {
  const std::string request_body = "token-request-bytes";
  const std::string response_body(kResponseMaxBodySize, 'a');
  SetResponse(response_body, /*expected_request_body = */ request_body);

  base::test::TestFuture<base::expected<std::string, TryGetTokensResult>>
      future;
  fetcher_->TryGetTokens(request_body, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get<0>();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result, response_body);
}

TEST_F(PrivateVerificationTokensFetcherTest, LargeResponseOverLimit) {
  const std::string request_body = "token-request-bytes";
  const std::string response_body(kResponseMaxBodySize + 1, 'a');
  SetResponse(response_body, /*expected_request_body = */ request_body);

  base::test::TestFuture<base::expected<std::string, TryGetTokensResult>>
      future;
  fetcher_->TryGetTokens(request_body, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get<0>();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, TryGetTokensError::kNetNotOk);
  EXPECT_EQ(result.error().network_error_code, net::ERR_INSUFFICIENT_RESOURCES);
}

TEST_F(PrivateVerificationTokensFetcherTest, OutOfMemory) {
  const std::string response_body = "";
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        auto head = network::mojom::URLResponseHead::New();
        test_url_loader_factory_.AddResponse(
            pvt_server_issue_url_, std::move(head), response_body,
            network::URLLoaderCompletionStatus(net::ERR_OUT_OF_MEMORY));
      }));

  base::test::TestFuture<base::expected<std::string, TryGetTokensResult>>
      future;
  fetcher_->TryGetTokens("", future.GetCallback());
  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get<0>();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, TryGetTokensError::kNetNotOk);
  EXPECT_EQ(result.error().network_error_code, net::ERR_OUT_OF_MEMORY);
}

TEST_F(PrivateVerificationTokensFetcherTest, DelayedRequestWithinLimit) {
  const std::string request_body = "some-request";
  const std::string response_body = "some-response";
  SetResponse(response_body, request_body,
              /*response_delay=*/kFetchTimeout - base::Seconds(1));

  base::test::TestFuture<base::expected<std::string, TryGetTokensResult>>
      future;
  fetcher_->TryGetTokens(request_body, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get<0>();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result, response_body);
}

TEST_F(PrivateVerificationTokensFetcherTest, RequestTimeout) {
  const std::string request_body = "some-request";
  const std::string response_body = "some-response";
  SetResponse(response_body, request_body, /*response_delay=*/kFetchTimeout);

  base::test::TestFuture<base::expected<std::string, TryGetTokensResult>>
      future;
  fetcher_->TryGetTokens(request_body, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get<0>();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, TryGetTokensError::kNetNotOk);
  EXPECT_EQ(result.error().network_error_code, net::ERR_TIMED_OUT);
}

TEST_F(PrivateVerificationTokensFetcherTest, TrafficAnnotation) {
  base::RunLoop run_loop;
  fetcher_->TryGetTokens(
      "dummy_body",
      base::IgnoreArgs<base::expected<std::string, TryGetTokensResult>>(
          run_loop.QuitClosure()));

  run_loop.Run();
  const std::vector<network::TestURLLoaderFactory::PendingRequest>& pending =
      *test_url_loader_factory_.pending_requests();
  ASSERT_EQ(pending.size(), 1u);

  net::NetworkTrafficAnnotationTag expected_tag =
      net::DefineNetworkTrafficAnnotation(
          "private_verification_tokens_service_get_tokens", "");
  EXPECT_EQ(pending[0].traffic_annotation.unique_id_hash_code,
            expected_tag.unique_id_hash_code);
}

TEST(PrivateVerificationTokensFetcherCreateTest, NullURLLoaderFactory) {
  EXPECT_THAT(PrivateVerificationTokensFetcher::Create(
                  GURL("http://example.com"), nullptr),
              testing::IsNull());
}

TEST(PrivateVerificationTokensFetcherCreateTest, InvalidURL) {
  base::test::TaskEnvironment task_environment;
  network::TestURLLoaderFactory test_url_loader_factory;
  EXPECT_THAT(
      PrivateVerificationTokensFetcher::Create(
          GURL(), test_url_loader_factory.GetSafeWeakWrapper()->Clone()),
      testing::IsNull());
}

}  // namespace

}  // namespace private_verification_tokens
