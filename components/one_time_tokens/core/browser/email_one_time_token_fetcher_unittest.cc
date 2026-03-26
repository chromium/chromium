// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/email_one_time_token_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64url.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/one_time_tokens/core/browser/fetch_email_one_time_token_request.pb.h"
#include "components/one_time_tokens/core/browser/fetch_email_one_time_token_response.pb.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

namespace {
constexpr char kOneTimeToken[] = "123456";
constexpr char kEncryptedMessageReference[] = "encrypted_reference";
constexpr char kServiceUrl[] =
    "https://onetimetoken.pa.googleapis.com/v1/onetimetokens:fetchEmail";
}  // namespace

class EmailOneTimeTokenFetcherTest : public testing::Test {
 public:
  EmailOneTimeTokenFetcherTest() = default;
  ~EmailOneTimeTokenFetcherTest() override = default;

  void SetUp() override {
    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
  }

 protected:
  std::unique_ptr<EmailOneTimeTokenFetcher> CreateFetcher() {
    return std::make_unique<EmailOneTimeTokenFetcher>(
        test_url_loader_factory_->GetSafeWeakWrapper(),
        kEncryptedMessageReference);
  }

  std::string CreateValidResponseString() {
    ::google::internal::chrome::passwords::onetimetoken::v1::
        FetchEmailOneTimeTokenResponse response;
    response.mutable_one_time_password()->set_one_time_password(kOneTimeToken);
    return response.SerializeAsString();
  }

  std::string CreateResponseWithoutToken() {
    ::google::internal::chrome::passwords::onetimetoken::v1::
        FetchEmailOneTimeTokenResponse response;
    return response.SerializeAsString();
  }

  std::string GetExpectedUrl() {
    std::string encoded_reference;
    base::Base64UrlEncode(kEncryptedMessageReference,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &encoded_reference);
    return net::AppendQueryParameter(GURL(kServiceUrl),
                                     "encryptedMessageReference",
                                     encoded_reference)
        .spec();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
};

// Tests the happy path of the email one time token fetcher.
TEST_F(EmailOneTimeTokenFetcherTest, Success) {
  std::unique_ptr<EmailOneTimeTokenFetcher> fetcher = CreateFetcher();
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;

  fetcher->Start(future.GetCallback());

  const network::ResourceRequest* pending_request =
      &test_url_loader_factory_->GetPendingRequest(0)->request;
  EXPECT_EQ(pending_request->method, "GET");

  EXPECT_EQ(pending_request->url.spec(), GetExpectedUrl());

  test_url_loader_factory_->AddResponse(pending_request->url.spec(),
                                        CreateValidResponseString());

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->value(), kOneTimeToken);
  EXPECT_EQ(result->type(), OneTimeTokenType::kGmail);
}

// Tests that an error is returned when the network request to the Gmail OTP
// endpoint fails.
TEST_F(EmailOneTimeTokenFetcherTest, NetworkError) {
  std::unique_ptr<EmailOneTimeTokenFetcher> fetcher = CreateFetcher();
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;

  fetcher->Start(future.GetCallback());

  ASSERT_TRUE(test_url_loader_factory_->IsPending(GetExpectedUrl()));
  test_url_loader_factory_->AddResponse(GetExpectedUrl(), "",
                                        net::HTTP_NOT_FOUND);

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            OneTimeTokenRetrievalError::kGmailOtpBackendNetworkError);
}

// Tests that an error is returned when the response proto is invalid.
TEST_F(EmailOneTimeTokenFetcherTest, InvalidResponseProto) {
  std::unique_ptr<EmailOneTimeTokenFetcher> fetcher = CreateFetcher();
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;

  fetcher->Start(future.GetCallback());

  ASSERT_TRUE(test_url_loader_factory_->IsPending(GetExpectedUrl()));
  test_url_loader_factory_->AddResponse(GetExpectedUrl(),
                                        "invalid_proto_content");

  auto result = future.Get();
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

  ASSERT_TRUE(test_url_loader_factory_->IsPending(GetExpectedUrl()));
  test_url_loader_factory_->AddResponse(GetExpectedUrl(),
                                        CreateResponseWithoutToken());

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            OneTimeTokenRetrievalError::kGmailOtpBackendInvalidResponse);
}

}  // namespace one_time_tokens
