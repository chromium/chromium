// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"

#include <optional>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/one_time_tokens/core/browser/fetch_email_one_time_token_response.pb.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

class GmailOtpBackendImplTest : public testing::Test {
 public:
  GmailOtpBackendImplTest()
      : backend_(test_url_loader_factory_.GetSafeWeakWrapper()) {}

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  GmailOtpBackendImpl backend_;
};

// Tests a successful retrieval of an OTP from Gmail.
TEST_F(GmailOtpBackendImplTest, SubscribeAndGetToken) {
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = backend_.Subscribe(
      base::Time::Now() + base::Minutes(1), future.GetRepeatingCallback());

  backend_.OnIncomingOneTimeTokenBackendTickle(
      GmailOtpBackend::EncryptedMessageReference("encrypted_reference"));

  ::google::internal::chrome::passwords::onetimetoken::v1::
      FetchEmailOneTimeTokenResponse response;
  response.mutable_one_time_password()->set_one_time_password("123456");

  std::string encoded_reference = base::Base64Encode("encrypted_reference");
  std::string url = net::AppendQueryParameter(
                        GURL("https://onetimetoken.pa.googleapis.com/v1/"
                             "onetimetokens:fetchEmail"),
                        "encryptedMessageReference", encoded_reference)
                        .spec();
  test_url_loader_factory_.AddResponse(url, response.SerializeAsString());

  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>& result =
      future.Get();

  ASSERT_TRUE(result.has_value());

  const OneTimeToken& token = result.value();
  EXPECT_EQ(token.type(), OneTimeTokenType::kGmail);
  EXPECT_EQ(token.value(), "123456");
  EXPECT_FALSE(token.on_device_arrival_time().is_null());
}

// Tests no backend calls are issued when there are no subscribers.
TEST_F(GmailOtpBackendImplTest, NoSubscriberNoBackendCall) {
  // No subscription created.

  backend_.OnIncomingOneTimeTokenBackendTickle(
      GmailOtpBackend::EncryptedMessageReference("encrypted_reference"));

  // Verify that no network request was made.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

// Tests that no new backend calls are issued when there is already a pending
// request.
// TODO(crbug.com/478840436): This documents current but undesired behavior.
// Going forward we want the subscriber to be able to received OTPs for "ref1"
// and "ref2".
TEST_F(GmailOtpBackendImplTest, PendingRequestNoNewBackendCall) {
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = backend_.Subscribe(
      base::Time::Now() + base::Minutes(1), future.GetRepeatingCallback());

  // First tickle starts a request.
  backend_.OnIncomingOneTimeTokenBackendTickle(
      GmailOtpBackend::EncryptedMessageReference("ref1"));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  // Second tickle should be ignored while a request is pending.
  backend_.OnIncomingOneTimeTokenBackendTickle(
      GmailOtpBackend::EncryptedMessageReference("ref2"));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
}

}  // namespace one_time_tokens
