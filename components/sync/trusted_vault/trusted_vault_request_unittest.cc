// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_request.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "google_apis/gaia/core_account_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::IsEmpty;
using testing::Ne;
using testing::NotNull;
using testing::Pointee;

const char kAccessToken[] = "access_token";
const char kRequestUrl[] = "https://test.com/test";
const char kRequestUrlWithAlternateOutputProto[] =
    "https://test.com/test?alt=proto";
const char kResponseBody[] = "response_body";

MATCHER(HasValidAccessToken, "") {
  const network::TestURLLoaderFactory::PendingRequest& pending_request = arg;
  std::string access_token_header;
  pending_request.request.headers.GetHeader("Authorization",
                                            &access_token_header);
  return access_token_header == base::StringPrintf("Bearer %s", kAccessToken);
}

class FakeTrustedVaultAccessTokenFetcher
    : public TrustedVaultAccessTokenFetcher {
 public:
  explicit FakeTrustedVaultAccessTokenFetcher(
      const absl::optional<std::string>& access_token)
      : access_token_(access_token) {}
  ~FakeTrustedVaultAccessTokenFetcher() override = default;

  void FetchAccessToken(const CoreAccountId& account_id,
                        TokenCallback callback) override {
    absl::optional<signin::AccessTokenInfo> access_token_info;
    if (access_token_) {
      access_token_info = signin::AccessTokenInfo(
          *access_token_, base::Time::Now() + base::Hours(1),
          /*id_token=*/std::string());
    }
    std::move(callback).Run(access_token_info);
  }

 private:
  const absl::optional<std::string> access_token_;
};

class TrustedVaultRequestTest : public testing::Test {
 public:
  TrustedVaultRequestTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  std::unique_ptr<TrustedVaultRequest> StartNewRequestWithAccessToken(
      const absl::optional<std::string>& access_token,
      TrustedVaultRequest::HttpMethod http_method,
      const absl::optional<std::string>& request_body,
      TrustedVaultRequest::CompletionCallback completion_callback) {
    const CoreAccountId account_id = CoreAccountId::FromGaiaId("user_id");
    FakeTrustedVaultAccessTokenFetcher access_token_fetcher(access_token);

    auto request = std::make_unique<TrustedVaultRequest>(
        http_method, GURL(kRequestUrl), request_body,
        shared_url_loader_factory_,
        TrustedVaultURLFetchReasonForUMA::kUnspecified);
    request->FetchAccessTokenAndSendRequest(account_id, &access_token_fetcher,
                                            std::move(completion_callback));
    return request;
  }

  bool RespondToHttpRequest(
      net::Error error,
      absl::optional<net::HttpStatusCode> response_http_code,
      const std::string& response_body) {
    network::mojom::URLResponseHeadPtr response_head;
    if (response_http_code.has_value()) {
      response_head = network::CreateURLResponseHead(*response_http_code);
    } else {
      response_head = network::mojom::URLResponseHead::New();
    }
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kRequestUrlWithAlternateOutputProto),
        network::URLLoaderCompletionStatus(error), std::move(response_head),
        response_body);
  }

  network::TestURLLoaderFactory::PendingRequest* GetPendingRequest() {
    return test_url_loader_factory_.GetPendingRequest(/*index=*/0);
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

}  // namespace

TEST_F(TrustedVaultRequestTest, ShouldSendGetRequestAndHandleSuccess) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  base::HistogramTester histogram_tester;
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      kAccessToken, TrustedVaultRequest::HttpMethod::kGet,
      /*request_body=*/absl::nullopt, completion_callback.Get());

  histogram_tester.ExpectUniqueSample(
      /*name=*/"Sync.TrustedVaultAccessTokenFetchSuccess",
      /*sample=*/true,
      /*expected_bucket_count=*/1);

  network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingRequest();
  EXPECT_THAT(pending_request, Pointee(HasValidAccessToken()));

  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("GET"));
  EXPECT_THAT(resource_request.url,
              Eq(GURL(kRequestUrlWithAlternateOutputProto)));
  EXPECT_THAT(network::GetUploadData(resource_request), IsEmpty());

  // |completion_callback| should be called after receiving response.
  EXPECT_CALL(
      completion_callback,
      Run(TrustedVaultRequest::HttpStatus::kSuccess, Eq(kResponseBody)));
  EXPECT_TRUE(RespondToHttpRequest(net::OK, net::HTTP_OK, kResponseBody));
}

TEST_F(TrustedVaultRequestTest,
       ShouldSendPostRequestWithoutPayloadAndHandleSuccess) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      kAccessToken, TrustedVaultRequest::HttpMethod::kPost,
      /*request_body=*/absl::nullopt, completion_callback.Get());

  network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingRequest();
  EXPECT_THAT(pending_request, Pointee(HasValidAccessToken()));

  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("POST"));
  EXPECT_THAT(resource_request.url,
              Eq(GURL(kRequestUrlWithAlternateOutputProto)));
  EXPECT_THAT(network::GetUploadData(resource_request), IsEmpty());

  // |completion_callback| should be called after receiving response.
  EXPECT_CALL(
      completion_callback,
      Run(TrustedVaultRequest::HttpStatus::kSuccess, Eq(kResponseBody)));
  EXPECT_TRUE(RespondToHttpRequest(net::OK, net::HTTP_OK, kResponseBody));
}

TEST_F(TrustedVaultRequestTest,
       ShouldSendPostRequestWithPayloadAndHandleSuccess) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  const std::string kRequestBody = "Request body";
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      kAccessToken, TrustedVaultRequest::HttpMethod::kPost, kRequestBody,
      completion_callback.Get());

  network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingRequest();
  EXPECT_THAT(pending_request, Pointee(HasValidAccessToken()));

  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("POST"));
  EXPECT_THAT(resource_request.url,
              Eq(GURL(kRequestUrlWithAlternateOutputProto)));
  EXPECT_THAT(network::GetUploadData(resource_request), Eq(kRequestBody));

  // |completion_callback| should be called after receiving response.
  EXPECT_CALL(
      completion_callback,
      Run(TrustedVaultRequest::HttpStatus::kSuccess, Eq(kResponseBody)));
  EXPECT_TRUE(RespondToHttpRequest(net::OK, net::HTTP_OK, kResponseBody));
}

TEST_F(TrustedVaultRequestTest, ShouldHandleNetworkFailures) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      kAccessToken, TrustedVaultRequest::HttpMethod::kGet,
      /*request_body=*/absl::nullopt, completion_callback.Get());

  // |completion_callback| should be called after receiving response.
  EXPECT_CALL(completion_callback,
              Run(TrustedVaultRequest::HttpStatus::kOtherError, _));
  EXPECT_TRUE(RespondToHttpRequest(net::ERR_FAILED, absl::nullopt,
                                   /*response_body=*/std::string()));
}

TEST_F(TrustedVaultRequestTest, ShouldHandleHttpErrors) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      kAccessToken, TrustedVaultRequest::HttpMethod::kGet,
      /*request_body=*/absl::nullopt, completion_callback.Get());

  // |completion_callback| should be called after receiving response.
  EXPECT_CALL(completion_callback,
              Run(TrustedVaultRequest::HttpStatus::kOtherError, _));
  EXPECT_TRUE(RespondToHttpRequest(net::OK, net::HTTP_INTERNAL_SERVER_ERROR,
                                   /*response_body=*/""));
}

TEST_F(TrustedVaultRequestTest, ShouldHandleBadRequestStatus) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      kAccessToken, TrustedVaultRequest::HttpMethod::kGet,
      /*request_body=*/absl::nullopt, completion_callback.Get());

  // |completion_callback| should be called after receiving response.
  EXPECT_CALL(completion_callback,
              Run(TrustedVaultRequest::HttpStatus::kBadRequest, _));
  EXPECT_TRUE(RespondToHttpRequest(net::OK, net::HTTP_BAD_REQUEST,
                                   /*response_body=*/""));
}

TEST_F(TrustedVaultRequestTest,
       ShouldHandleConflictStatusAndPopulateResponseBody) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      kAccessToken, TrustedVaultRequest::HttpMethod::kGet,
      /*request_body=*/absl::nullopt, completion_callback.Get());

  // |completion_callback| should be called after receiving response.
  EXPECT_CALL(completion_callback,
              Run(TrustedVaultRequest::HttpStatus::kConflict, kResponseBody));
  EXPECT_TRUE(RespondToHttpRequest(net::OK, net::HTTP_CONFLICT, kResponseBody));
}

TEST_F(TrustedVaultRequestTest, ShouldHandleNotFoundStatus) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      kAccessToken, TrustedVaultRequest::HttpMethod::kGet,
      /*request_body=*/absl::nullopt, completion_callback.Get());

  // |completion_callback| should be called after receiving response.
  EXPECT_CALL(completion_callback,
              Run(TrustedVaultRequest::HttpStatus::kNotFound, _));
  EXPECT_TRUE(RespondToHttpRequest(net::OK, net::HTTP_NOT_FOUND,
                                   /*response_body=*/""));
}

TEST_F(TrustedVaultRequestTest, ShouldRetryUponNetworkChange) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      kAccessToken, TrustedVaultRequest::HttpMethod::kGet,
      /*request_body=*/absl::nullopt, completion_callback.Get());

  // Mimic network change error for the first request.
  EXPECT_CALL(completion_callback, Run).Times(0);
  EXPECT_TRUE(RespondToHttpRequest(net::ERR_NETWORK_CHANGED, net::HTTP_OK,
                                   /*response_body=*/""));
  testing::Mock::VerifyAndClearExpectations(&completion_callback);

  // Second request should be sent, mimic its success.
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingRequest();
  EXPECT_THAT(pending_request, NotNull());

  EXPECT_CALL(completion_callback,
              Run(TrustedVaultRequest::HttpStatus::kSuccess, kResponseBody));
  EXPECT_TRUE(RespondToHttpRequest(net::OK, net::HTTP_OK, kResponseBody));
}

TEST_F(TrustedVaultRequestTest, ShouldHandleAccessTokenFetchingFailures) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  base::HistogramTester histogram_tester;
  // Access token fetching failure propagated immediately in this test, so
  // |completion_callback| should be called immediately as well.
  EXPECT_CALL(
      completion_callback,
      Run(TrustedVaultRequest::HttpStatus::kAccessTokenFetchingFailure, _));
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      /*access_token=*/absl::nullopt, TrustedVaultRequest::HttpMethod::kGet,
      /*request_body=*/absl::nullopt, completion_callback.Get());
  histogram_tester.ExpectUniqueSample(
      /*name=*/"Sync.TrustedVaultAccessTokenFetchSuccess",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
}

}  // namespace syncer
