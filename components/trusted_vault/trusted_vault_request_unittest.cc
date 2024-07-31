// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_request.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/trusted_vault/test/fake_trusted_vault_access_token_fetcher.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "google_apis/gaia/core_account_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

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
  std::string access_token_header =
      pending_request.request.headers.GetHeader("Authorization")
          .value_or(std::string());
  return access_token_header == base::StringPrintf("Bearer %s", kAccessToken);
}

signin::AccessTokenInfo MakeAccessTokenInfo(const std::string& access_token) {
  return signin::AccessTokenInfo(
      access_token,
      /*expiration_time_param=*/base::Time::Now() + base::Hours(1),
      /*id_token=*/std::string());
}

class TrustedVaultRequestTest : public testing::Test {
 public:
  TrustedVaultRequestTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  std::unique_ptr<TrustedVaultRequest>
  StartNewRequestWithAccessTokenAndRetriesDuration(
      const std::string& access_token,
      TrustedVaultRequest::HttpMethod http_method,
      const std::optional<std::string>& request_body,
      base::TimeDelta max_retry_duration,
      TrustedVaultRequest::CompletionCallback completion_callback) {
    const CoreAccountId account_id = CoreAccountId::FromGaiaId("user_id");
    FakeTrustedVaultAccessTokenFetcher access_token_fetcher(
        MakeAccessTokenInfo(access_token));

    auto request = std::make_unique<TrustedVaultRequest>(
        account_id, http_method, GURL(kRequestUrl), request_body,
        max_retry_duration, shared_url_loader_factory_,
        std::make_unique<FakeTrustedVaultAccessTokenFetcher>(
            MakeAccessTokenInfo(access_token)),
        /*record_fetch_status_callback=*/base::DoNothing());
    request->FetchAccessTokenAndSendRequest(std::move(completion_callback));
    return request;
  }

  std::unique_ptr<TrustedVaultRequest> StartNewRequestWithAccessToken(
      const std::string& access_token,
      TrustedVaultRequest::HttpMethod http_method,
      const std::optional<std::string>& request_body,
      TrustedVaultRequest::CompletionCallback completion_callback) {
    return StartNewRequestWithAccessTokenAndRetriesDuration(
        access_token, http_method, request_body,
        /*max_retry_duration=*/base::Seconds(0),
        std::move(completion_callback));
  }

  std::unique_ptr<TrustedVaultRequest> StartNewRequestWithAccessTokenError(
      TrustedVaultAccessTokenFetcher::FetchingError error,
      TrustedVaultRequest::CompletionCallback completion_callback) {
    const CoreAccountId account_id = CoreAccountId::FromGaiaId("user_id");

    auto request = std::make_unique<TrustedVaultRequest>(
        account_id, TrustedVaultRequest::HttpMethod::kGet, GURL(kRequestUrl),
        /*serialized_request_proto=*/std::nullopt,
        /*max_retry_duration=*/base::Seconds(0), shared_url_loader_factory_,
        std::make_unique<FakeTrustedVaultAccessTokenFetcher>(
            base::unexpected{error}),
        /*record_fetch_status_callback=*/base::DoNothing());
    request->FetchAccessTokenAndSendRequest(std::move(completion_callback));
    return request;
  }

  bool RespondToHttpRequest(
      net::Error error,
      std::optional<net::HttpStatusCode> response_http_code,
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

  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

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
      /*request_body=*/std::nullopt, completion_callback.Get());

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
      /*request_body=*/std::nullopt, completion_callback.Get());

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
       ShouldSendPatchRequestWithoutPayloadAndHandleSuccess) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      kAccessToken, TrustedVaultRequest::HttpMethod::kPatch,
      /*request_body=*/std::nullopt, completion_callback.Get());

  network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingRequest();
  EXPECT_THAT(pending_request, Pointee(HasValidAccessToken()));

  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("PATCH"));
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
      /*request_body=*/std::nullopt, completion_callback.Get());

  // |completion_callback| should be called after receiving response.
  EXPECT_CALL(completion_callback,
              Run(TrustedVaultRequest::HttpStatus::kNetworkError, _));
  EXPECT_TRUE(RespondToHttpRequest(net::ERR_FAILED, std::nullopt,
                                   /*response_body=*/std::string()));
}

TEST_F(TrustedVaultRequestTest, ShouldHandleHttpErrors) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  std::unique_ptr<TrustedVaultRequest> request = StartNewRequestWithAccessToken(
      kAccessToken, TrustedVaultRequest::HttpMethod::kGet,
      /*request_body=*/std::nullopt, completion_callback.Get());

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
      /*request_body=*/std::nullopt, completion_callback.Get());

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
      /*request_body=*/std::nullopt, completion_callback.Get());

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
      /*request_body=*/std::nullopt, completion_callback.Get());

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
      /*request_body=*/std::nullopt, completion_callback.Get());

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

TEST_F(TrustedVaultRequestTest, ShouldRetryUponTransientErrorAndHandleSuccess) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  std::unique_ptr<TrustedVaultRequest> request =
      StartNewRequestWithAccessTokenAndRetriesDuration(
          kAccessToken, TrustedVaultRequest::HttpMethod::kGet,
          /*request_body=*/std::nullopt,
          /*max_retry_duration=*/base::Minutes(1), completion_callback.Get());

  // Mimic network error for the first request.
  EXPECT_CALL(completion_callback, Run).Times(0);
  EXPECT_TRUE(RespondToHttpRequest(net::ERR_DNS_REQUEST_CANCELLED, net::HTTP_OK,
                                   /*response_body=*/""));
  testing::Mock::VerifyAndClearExpectations(&completion_callback);

  // Forward time and expect the second attempt.
  task_environment().FastForwardBy(base::Seconds(15));
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingRequest();
  EXPECT_THAT(pending_request, NotNull());

  EXPECT_CALL(completion_callback,
              Run(TrustedVaultRequest::HttpStatus::kSuccess, kResponseBody));
  EXPECT_TRUE(RespondToHttpRequest(net::OK, net::HTTP_OK, kResponseBody));
}

TEST_F(TrustedVaultRequestTest, ShouldStopRetryingAndReportTransientError) {
  base::MockCallback<TrustedVaultRequest::CompletionCallback>
      completion_callback;
  std::unique_ptr<TrustedVaultRequest> request =
      StartNewRequestWithAccessTokenAndRetriesDuration(
          kAccessToken, TrustedVaultRequest::HttpMethod::kGet,
          /*request_body=*/std::nullopt,
          /*max_retry_duration=*/base::Minutes(1), completion_callback.Get());

  // Mimic network error for the first request with significant delay,
  // sufficient to stop retries.
  task_environment().FastForwardBy(base::Minutes(2));
  EXPECT_CALL(completion_callback,
              Run(TrustedVaultRequest::HttpStatus::kNetworkError, _));
  EXPECT_TRUE(RespondToHttpRequest(net::ERR_DNS_REQUEST_CANCELLED, net::HTTP_OK,
                                   /*response_body=*/""));
  testing::Mock::VerifyAndClearExpectations(&completion_callback);
}

TEST_F(TrustedVaultRequestTest, ShouldHandleAccessTokenFetchingFailures) {
  base::flat_map<TrustedVaultAccessTokenFetcher::FetchingError,
                 TrustedVaultRequest::HttpStatus>
      fetching_error_to_http_status = {
          {TrustedVaultAccessTokenFetcher::FetchingError::kTransientAuthError,
           TrustedVaultRequest::HttpStatus::kTransientAccessTokenFetchError},
          {TrustedVaultAccessTokenFetcher::FetchingError::kPersistentAuthError,
           TrustedVaultRequest::HttpStatus::kPersistentAccessTokenFetchError},
          {TrustedVaultAccessTokenFetcher::FetchingError::kNotPrimaryAccount,
           TrustedVaultRequest::HttpStatus::
               kPrimaryAccountChangeAccessTokenFetchError}};

  for (const auto& [fetching_error, expected_http_status] :
       fetching_error_to_http_status) {
    base::HistogramTester histogram_tester;

    base::MockCallback<TrustedVaultRequest::CompletionCallback>
        completion_callback;
    // Access token fetching failure propagated immediately in this test, so
    // |completion_callback| should be called immediately as well.
    EXPECT_CALL(completion_callback, Run(expected_http_status, _));
    std::unique_ptr<TrustedVaultRequest> request =
        StartNewRequestWithAccessTokenError(fetching_error,
                                            completion_callback.Get());
    histogram_tester.ExpectUniqueSample(
        /*name=*/"Sync.TrustedVaultAccessTokenFetchSuccess",
        /*sample=*/false,
        /*expected_bucket_count=*/1);
  }
}

}  // namespace trusted_vault
