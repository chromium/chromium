// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/email_one_time_token_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64url.h"
#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/one_time_tokens/core/browser/fetch_email_one_time_token_error_details.pb.h"
#include "components/one_time_tokens/core/browser/fetch_email_one_time_token_request.pb.h"
#include "components/one_time_tokens/core/browser/fetch_email_one_time_token_response.pb.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

using ::google::internal::chrome::passwords::onetimetoken::v1::
    FetchEmailOneTimeTokenErrorDetails;
using ::google::internal::chrome::passwords::onetimetoken::v1::
    FetchEmailOneTimeTokenResponse;
using ::google::internal::chrome::passwords::onetimetoken::v1::Proto3Any;
using ::google::internal::chrome::passwords::onetimetoken::v1::RpcStatus;

namespace {
constexpr char kTestEmail[] = "test@example.com";
constexpr char kTestAccessToken[] = "access_token";
constexpr char kOneTimeToken[] = "123456";
constexpr char kSenderAddress[] = "sender@example.com";
constexpr char kEncryptedMessageReference[] = "encrypted_reference";
constexpr char kServiceUrl[] =
    "https://onetimetoken.pa.googleapis.com/v1/onetimetokens:fetchEmail";
constexpr char kErrorDetailsTypeUrl[] =
    "type.googleapis.com/"
    "google.internal.chrome.passwords.onetimetoken.v1."
    "FetchEmailOneTimeTokenErrorDetails";
}  // namespace

class EmailOneTimeTokenFetcherTest : public testing::Test {
 public:
  EmailOneTimeTokenFetcherTest() = default;
  ~EmailOneTimeTokenFetcherTest() override = default;

  void SetUp() override {
    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();

    identity_test_env_->MakePrimaryAccountAvailable(
        kTestEmail, signin::ConsentLevel::kSignin);
  }

 protected:
  std::unique_ptr<EmailOneTimeTokenFetcher> CreateFetcher() {
    return std::make_unique<EmailOneTimeTokenFetcher>(
        test_url_loader_factory_->GetSafeWeakWrapper(),
        *identity_test_env_->identity_manager(), kEncryptedMessageReference);
  }

  void WaitForAccessTokenRequestAndRespondWithSuccess() {
    identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        kTestAccessToken, base::Time::Now() + base::Hours(1));
  }

  void WaitForAccessTokenRequestAndRespondWithError() {
    identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED));
  }

  std::string CreateValidResponseString() {
    FetchEmailOneTimeTokenResponse response;
    response.mutable_one_time_password()->set_one_time_password(kOneTimeToken);
    response.set_sender_address(kSenderAddress);
    return response.SerializeAsString();
  }

  std::string CreateResponseWithoutToken() {
    FetchEmailOneTimeTokenResponse response;
    response.set_sender_address(kSenderAddress);
    return response.SerializeAsString();
  }

  std::string CreateResponseWithoutSenderAddress() {
    FetchEmailOneTimeTokenResponse response;
    response.mutable_one_time_password()->set_one_time_password(kOneTimeToken);
    return response.SerializeAsString();
  }

  std::string GetExpectedUrl() {
    std::string encoded_reference;
    base::Base64UrlEncode(kEncryptedMessageReference,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &encoded_reference);
    GURL url = net::AppendQueryParameter(
        GURL(kServiceUrl), "encryptedMessageReference", encoded_reference);
    return net::AppendQueryParameter(url, "alt", "proto").spec();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
};

// Tests the happy path of the email one time token fetcher.
TEST_F(EmailOneTimeTokenFetcherTest, Success) {
  std::unique_ptr<EmailOneTimeTokenFetcher> fetcher = CreateFetcher();
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;

  fetcher->Start(future.GetCallback());
  task_environment_.FastForwardBy(base::Milliseconds(123));
  WaitForAccessTokenRequestAndRespondWithSuccess();

  const network::ResourceRequest* pending_request =
      &test_url_loader_factory_->GetPendingRequest(0)->request;
  EXPECT_EQ(pending_request->method, "GET");
  const std::optional<std::string> auth_header =
      pending_request->headers.GetHeader(
          net::HttpRequestHeaders::kAuthorization);
  ASSERT_TRUE(auth_header.has_value());
  EXPECT_EQ(*auth_header, "Bearer access_token");

  const std::optional<std::string> qos_header =
      pending_request->headers.GetHeader(
          kOneTimeTokenServiceCriticalityHeaderName);
  ASSERT_TRUE(qos_header.has_value());
  EXPECT_EQ(*qos_header, kOneTimeTokenServiceCriticalityHeaderValue);

  const std::optional<std::string> accept_header =
      pending_request->headers.GetHeader(net::HttpRequestHeaders::kAccept);
  ASSERT_TRUE(accept_header.has_value());
  EXPECT_EQ(*accept_header, "application/x-protobuf");

  EXPECT_EQ(pending_request->url.spec(), GetExpectedUrl());

  test_url_loader_factory_->AddResponse(pending_request->url.spec(),
                                        CreateValidResponseString());

  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>& result =
      future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->value(), kOneTimeToken);
  EXPECT_EQ(result->type(), OneTimeTokenType::kGmail);
  EXPECT_EQ(result->sender_address(), kSenderAddress);
  histogram_tester_.ExpectUniqueTimeSample(
      "Autofill.OneTimeTokens.Backend.Gmail.AuthLatency",
      base::Milliseconds(123), 1);
}

// Tests that an error is returned when user authentication fails.
TEST_F(EmailOneTimeTokenFetcherTest, AccessTokenError) {
  std::unique_ptr<EmailOneTimeTokenFetcher> fetcher = CreateFetcher();
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;

  fetcher->Start(future.GetCallback());
  task_environment_.FastForwardBy(base::Milliseconds(456));
  WaitForAccessTokenRequestAndRespondWithError();

  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>& result =
      future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            OneTimeTokenRetrievalError::kGmailOtpBackendAuthError);
  histogram_tester_.ExpectUniqueTimeSample(
      "Autofill.OneTimeTokens.Backend.Gmail.AuthLatency",
      base::Milliseconds(456), 1);
}

// Tests that an error is returned when the network request to the Gmail OTP
// endpoint fails with an HTTP error and empty body.
TEST_F(EmailOneTimeTokenFetcherTest, NetworkError) {
  std::unique_ptr<EmailOneTimeTokenFetcher> fetcher = CreateFetcher();
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;

  fetcher->Start(future.GetCallback());
  WaitForAccessTokenRequestAndRespondWithSuccess();

  ASSERT_TRUE(test_url_loader_factory_->IsPending(GetExpectedUrl()));
  test_url_loader_factory_->AddResponse(GetExpectedUrl(), "",
                                        net::HTTP_NOT_FOUND);

  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>& result =
      future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            OneTimeTokenRetrievalError::kGmailOtpBackendInvalidResponse);
  histogram_tester_.ExpectTotalCount(
      "Autofill.OneTimeTokens.Backend.Gmail.NetworkLatency", 1);
}

// Tests that an error is returned when the network request fails entirely
// (nullopt body).
TEST_F(EmailOneTimeTokenFetcherTest, NetworkDisconnected) {
  std::unique_ptr<EmailOneTimeTokenFetcher> fetcher = CreateFetcher();
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;

  fetcher->Start(future.GetCallback());
  WaitForAccessTokenRequestAndRespondWithSuccess();

  ASSERT_TRUE(test_url_loader_factory_->IsPending(GetExpectedUrl()));
  test_url_loader_factory_->AddResponse(
      GURL(GetExpectedUrl()), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_FAILED));

  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>& result =
      future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            OneTimeTokenRetrievalError::kGmailOtpBackendNetworkError);
  histogram_tester_.ExpectTotalCount(
      "Autofill.OneTimeTokens.Backend.Gmail.NetworkLatency", 1);
}

// Tests that an error is returned when the response proto is invalid.
TEST_F(EmailOneTimeTokenFetcherTest, InvalidResponseProto) {
  std::unique_ptr<EmailOneTimeTokenFetcher> fetcher = CreateFetcher();
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;

  fetcher->Start(future.GetCallback());
  WaitForAccessTokenRequestAndRespondWithSuccess();

  ASSERT_TRUE(test_url_loader_factory_->IsPending(GetExpectedUrl()));
  test_url_loader_factory_->AddResponse(GetExpectedUrl(),
                                        "invalid_proto_content");

  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>& result =
      future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            OneTimeTokenRetrievalError::kGmailOtpBackendInvalidResponse);
}

// Tests that an error is returned when the response proto is structurally
// valid but does not contain a one time token.
TEST_F(EmailOneTimeTokenFetcherTest, ResponseWithoutToken) {
  std::unique_ptr<EmailOneTimeTokenFetcher> fetcher = CreateFetcher();
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;

  fetcher->Start(future.GetCallback());
  WaitForAccessTokenRequestAndRespondWithSuccess();

  ASSERT_TRUE(test_url_loader_factory_->IsPending(GetExpectedUrl()));
  test_url_loader_factory_->AddResponse(GetExpectedUrl(),
                                        CreateResponseWithoutToken());

  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>& result =
      future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            OneTimeTokenRetrievalError::kGmailOtpBackendInvalidResponse);
  auto unused = future.Get();
}

// Tests that an error is returned when the response proto is structurally
// valid and contains a token, but has an empty `sender_address`.
TEST_F(EmailOneTimeTokenFetcherTest, ResponseWithoutSenderAddress) {
  std::unique_ptr<EmailOneTimeTokenFetcher> fetcher = CreateFetcher();
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;

  fetcher->Start(future.GetCallback());
  WaitForAccessTokenRequestAndRespondWithSuccess();

  ASSERT_TRUE(test_url_loader_factory_->IsPending(GetExpectedUrl()));
  test_url_loader_factory_->AddResponse(GetExpectedUrl(),
                                        CreateResponseWithoutSenderAddress());

  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>& result =
      future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            OneTimeTokenRetrievalError::kGmailOtpBackendInvalidResponse);
}

// Tests that the fetcher retries on transient errors (like HTTP 500) and
// eventually succeeds if a subsequent attempt is successful.
TEST_F(EmailOneTimeTokenFetcherTest, RetriesOnTransientError) {
  std::unique_ptr<EmailOneTimeTokenFetcher> fetcher = CreateFetcher();
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;

  fetcher->Start(future.GetCallback());
  WaitForAccessTokenRequestAndRespondWithSuccess();

  ASSERT_TRUE(test_url_loader_factory_->IsPending(GetExpectedUrl()));

  // Add a 500 error response first.
  test_url_loader_factory_->AddResponse(GetExpectedUrl(), "",
                                        net::HTTP_INTERNAL_SERVER_ERROR);

  // SimpleURLLoader should automatically retry.
  // TestURLLoaderFactory will consume the first response and wait for the next
  // request for the same URL.
  // We provide a successful response for the retry attempt.
  test_url_loader_factory_->AddResponse(GetExpectedUrl(),
                                        CreateValidResponseString());

  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>& result =
      future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->value(), kOneTimeToken);
  EXPECT_EQ(test_url_loader_factory_->total_requests(), 2u);
}

// Tests that a backend error mapped to a granular user consent error is
// properly converted.
TEST_F(EmailOneTimeTokenFetcherTest, BackendErrorConsentRequired) {
  std::unique_ptr<EmailOneTimeTokenFetcher> fetcher = CreateFetcher();
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;

  fetcher->Start(future.GetCallback());
  WaitForAccessTokenRequestAndRespondWithSuccess();

  ASSERT_TRUE(test_url_loader_factory_->IsPending(GetExpectedUrl()));

  RpcStatus rpc_status;
  rpc_status.set_code(403);
  rpc_status.set_message("Consent required");

  FetchEmailOneTimeTokenErrorDetails error_details;
  error_details.add_reason_code(
      FetchEmailOneTimeTokenErrorDetails::
          SMART_FEATURES_IN_GMAIL_CONSENT_IS_REQUIRED);

  Proto3Any* detail = rpc_status.add_details();
  detail->set_type_url(kErrorDetailsTypeUrl);
  detail->set_value(error_details.SerializeAsString());

  test_url_loader_factory_->AddResponse(
      GetExpectedUrl(), rpc_status.SerializeAsString(), net::HTTP_FORBIDDEN);

  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>& result =
      future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            OneTimeTokenRetrievalError::
                kGmailOtpBackendSmartFeaturesInGmailConsentRequired);
}

// Tests that an error is returned when the response is a backend HTTP error but
// the RpcStatus proto is unparsable.
TEST_F(EmailOneTimeTokenFetcherTest, BackendErrorUnparsable) {
  std::unique_ptr<EmailOneTimeTokenFetcher> fetcher = CreateFetcher();
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;

  fetcher->Start(future.GetCallback());
  WaitForAccessTokenRequestAndRespondWithSuccess();

  ASSERT_TRUE(test_url_loader_factory_->IsPending(GetExpectedUrl()));

  test_url_loader_factory_->AddResponse(GetExpectedUrl(), "unparsable_payload",
                                        net::HTTP_INTERNAL_SERVER_ERROR);

  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>& result =
      future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            OneTimeTokenRetrievalError::kGmailOtpBackendInvalidResponse);
}

// Tests that an error is returned when the response is a backend HTTP error,
// RpcStatus parses, but it does not contain the required type_url.
TEST_F(EmailOneTimeTokenFetcherTest, BackendErrorWithoutErrorDetails) {
  std::unique_ptr<EmailOneTimeTokenFetcher> fetcher = CreateFetcher();
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;

  fetcher->Start(future.GetCallback());
  WaitForAccessTokenRequestAndRespondWithSuccess();

  ASSERT_TRUE(test_url_loader_factory_->IsPending(GetExpectedUrl()));

  RpcStatus rpc_status;
  rpc_status.set_code(403);
  rpc_status.set_message("Consent required");
  // Missing details.

  test_url_loader_factory_->AddResponse(
      GetExpectedUrl(), rpc_status.SerializeAsString(), net::HTTP_FORBIDDEN);

  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>& result =
      future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            OneTimeTokenRetrievalError::kGmailOtpBackendInvalidResponse);
}

// Tests that an error is returned when the response is a backend HTTP error,
// OneTimeTokenErrorDetails parses, but it has no reason codes.
TEST_F(EmailOneTimeTokenFetcherTest, BackendErrorWithEmptyReasonCodes) {
  std::unique_ptr<EmailOneTimeTokenFetcher> fetcher = CreateFetcher();
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;

  fetcher->Start(future.GetCallback());
  WaitForAccessTokenRequestAndRespondWithSuccess();

  ASSERT_TRUE(test_url_loader_factory_->IsPending(GetExpectedUrl()));

  RpcStatus rpc_status;
  rpc_status.set_code(403);
  rpc_status.set_message("Consent required");

  FetchEmailOneTimeTokenErrorDetails error_details;
  // Intentionally leaving reason_codes empty.

  Proto3Any* detail = rpc_status.add_details();
  detail->set_type_url(kErrorDetailsTypeUrl);
  detail->set_value(error_details.SerializeAsString());

  test_url_loader_factory_->AddResponse(
      GetExpectedUrl(), rpc_status.SerializeAsString(), net::HTTP_FORBIDDEN);

  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>& result =
      future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            OneTimeTokenRetrievalError::kGmailOtpBackendInvalidResponse);
}

// Tests that an error is returned when the response has no HTTP response code,
// but has an RpcStatus body.
TEST_F(EmailOneTimeTokenFetcherTest, NoResponseCodeWithValidBody) {
  std::unique_ptr<EmailOneTimeTokenFetcher> fetcher = CreateFetcher();
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;

  fetcher->Start(future.GetCallback());
  WaitForAccessTokenRequestAndRespondWithSuccess();

  ASSERT_TRUE(test_url_loader_factory_->IsPending(GetExpectedUrl()));

  RpcStatus rpc_status;
  rpc_status.set_code(403);
  rpc_status.set_message("Consent required");

  FetchEmailOneTimeTokenErrorDetails error_details;
  error_details.add_reason_code(
      FetchEmailOneTimeTokenErrorDetails::
          SMART_FEATURES_IN_GMAIL_CONSENT_IS_REQUIRED);

  Proto3Any* detail = rpc_status.add_details();
  detail->set_type_url(kErrorDetailsTypeUrl);
  detail->set_value(error_details.SerializeAsString());

  test_url_loader_factory_->AddResponse(
      GURL(GetExpectedUrl()), network::mojom::URLResponseHead::New(),
      rpc_status.SerializeAsString(),
      network::URLLoaderCompletionStatus(net::OK));

  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>& result =
      future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            OneTimeTokenRetrievalError::
                kGmailOtpBackendSmartFeaturesInGmailConsentRequired);
}

}  // namespace one_time_tokens
