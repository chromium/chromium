// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"

#include <memory>
#include <optional>

#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/one_time_tokens/core/browser/fetch_email_one_time_token_response.pb.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

class GmailOtpBackendImplTest : public testing::Test {
 public:
  GmailOtpBackendImplTest()
      : backend_(test_url_loader_factory_.GetSafeWeakWrapper(),
                 *identity_test_env_.identity_manager()) {}

  void SetUp() override {
    identity_test_env_.MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  GmailOtpBackendImpl backend_;
};

// Tests a successful retrieval of an OTP from Gmail.
TEST_F(GmailOtpBackendImplTest, SubscribeAndGetToken) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = backend_.Subscribe(
      base::Time::Now() + base::Minutes(1), future.GetRepeatingCallback());

  backend_.OnIncomingOneTimeTokenBackendNotification(
      OneTimeTokenBackendNotification(
          EncryptedMessageReference("encrypted_reference")));
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  ::google::internal::chrome::passwords::onetimetoken::v1::
      FetchEmailOneTimeTokenResponse response;
  response.mutable_one_time_password()->set_one_time_password("123456");

  std::string encoded_reference;
  base::Base64UrlEncode("encrypted_reference",
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_reference);
  const GURL url = net::AppendQueryParameter(
      GURL("https://onetimetoken.pa.googleapis.com/v1/"
           "onetimetokens:fetchEmail"),
      "encryptedMessageReference", encoded_reference);

  task_environment_.FastForwardBy(base::Milliseconds(500));

  test_url_loader_factory_.AddResponse(url.spec(),
                                       response.SerializeAsString());

  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>& result =
      future.Get();

  ASSERT_TRUE(result.has_value());

  const OneTimeToken& token = result.value();
  EXPECT_EQ(token.type(), OneTimeTokenType::kGmail);
  EXPECT_EQ(token.value(), "123456");
  EXPECT_FALSE(token.on_device_arrival_time().is_null());

  histogram_tester.ExpectUniqueSample(
      "Autofill.OneTimeTokens.Backend.Gmail.Success", true, 1);
  histogram_tester.ExpectTimeBucketCount(
      "Autofill.OneTimeTokens.Backend.Gmail.SuccessLatency",
      base::Milliseconds(500), 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OneTimeTokens.Backend.Gmail.HasActiveSubscription", true, 1);
}

// Tests a failed retrieval of an OTP from Gmail.
TEST_F(GmailOtpBackendImplTest, SubscribeAndGetTokenFailure) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = backend_.Subscribe(
      base::Time::Now() + base::Minutes(1), future.GetRepeatingCallback());

  backend_.OnIncomingOneTimeTokenBackendNotification(
      OneTimeTokenBackendNotification(
          EncryptedMessageReference("encrypted_reference_fail")));
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  std::string encoded_reference;
  base::Base64UrlEncode("encrypted_reference_fail",
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_reference);
  const GURL url = net::AppendQueryParameter(
      GURL("https://onetimetoken.pa.googleapis.com/v1/"
           "onetimetokens:fetchEmail"),
      "encryptedMessageReference", encoded_reference);

  task_environment_.FastForwardBy(base::Milliseconds(500));

  // Return an HTTP 500 to simulate a network error
  test_url_loader_factory_.AddResponse(url.spec(), "",
                                       net::HTTP_INTERNAL_SERVER_ERROR);

  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>& result =
      future.Get();

  ASSERT_FALSE(result.has_value());

  histogram_tester.ExpectUniqueSample(
      "Autofill.OneTimeTokens.Backend.Gmail.Success", false, 1);
  histogram_tester.ExpectTimeBucketCount(
      "Autofill.OneTimeTokens.Backend.Gmail.ErrorLatency",
      base::Milliseconds(500), 1);
}

// Tests no backend calls are issued when there are no subscribers.
TEST_F(GmailOtpBackendImplTest, NoSubscriberNoBackendCall) {
  // No subscription created.

  backend_.OnIncomingOneTimeTokenBackendNotification(
      OneTimeTokenBackendNotification(
          EncryptedMessageReference("encrypted_reference")));

  // Verify that no network request was made.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

// Tests that multiple backend calls can be issued for different references.
TEST_F(GmailOtpBackendImplTest, MultiplePendingRequestsAllowed) {
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = backend_.Subscribe(
      base::Time::Now() + base::Minutes(1), future.GetRepeatingCallback());

  // First tickle starts a request.
  backend_.OnIncomingOneTimeTokenBackendNotification(
      OneTimeTokenBackendNotification(EncryptedMessageReference("ref1")));
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  // Second tickle for a different reference should also start a request.
  backend_.OnIncomingOneTimeTokenBackendNotification(
      OneTimeTokenBackendNotification(EncryptedMessageReference("ref2")));
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 2);

  // Complete requests to avoid dangling pointers.
  std::string ref1_encoded;
  base::Base64UrlEncode("ref1", base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &ref1_encoded);
  test_url_loader_factory_.AddResponse(
      net::AppendQueryParameter(
          GURL("https://onetimetoken.pa.googleapis.com/v1/"
               "onetimetokens:fetchEmail"),
          "encryptedMessageReference", ref1_encoded)
          .spec(),
      "");
  std::string ref2_encoded;
  base::Base64UrlEncode("ref2", base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &ref2_encoded);
  test_url_loader_factory_.AddResponse(
      net::AppendQueryParameter(
          GURL("https://onetimetoken.pa.googleapis.com/v1/"
               "onetimetokens:fetchEmail"),
          "encryptedMessageReference", ref2_encoded)
          .spec(),
      "");
}

// Tests that the backend enforces the concurrency limit via the coordinator.
TEST_F(GmailOtpBackendImplTest, EnforcesConcurrencyLimit) {
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = backend_.Subscribe(
      base::Time::Now() + base::Minutes(1), future.GetRepeatingCallback());

  // Send 3 tickles and respond to their token requests.
  for (int i = 1; i <= 3; ++i) {
    backend_.OnIncomingOneTimeTokenBackendNotification(
        OneTimeTokenBackendNotification(
            EncryptedMessageReference("ref" + base::NumberToString(i))));
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        "access_token", base::Time::Now() + base::Hours(1));
  }

  // Now we should have 3 network requests in flight.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 3);

  // Send 2 more tickles.
  for (int i = 4; i <= 5; ++i) {
    backend_.OnIncomingOneTimeTokenBackendNotification(
        OneTimeTokenBackendNotification(
            EncryptedMessageReference("ref" + base::NumberToString(i))));
  }

  // No more access token requests should be pending as we hit the limit.
  EXPECT_EQ(identity_test_env_.GetPendingAccessTokenRequests().size(), 0u);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 3);
}

// Tests that the backend processes the queue when a request completes.
TEST_F(GmailOtpBackendImplTest, ProcessesQueueOnCompletion) {
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = backend_.Subscribe(
      base::Time::Now() + base::Minutes(1), future.GetRepeatingCallback());

  // Start 3 requests (fill the limit).
  for (int i = 1; i <= 3; ++i) {
    backend_.OnIncomingOneTimeTokenBackendNotification(
        OneTimeTokenBackendNotification(
            EncryptedMessageReference("ref" + base::NumberToString(i))));
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        "access_token", base::Time::Now() + base::Hours(1));
  }
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 3);

  // Add a 4th pending request.
  backend_.OnIncomingOneTimeTokenBackendNotification(
      OneTimeTokenBackendNotification(EncryptedMessageReference("ref4")));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 3);

  // Complete one request (ref1).
  std::string ref1_encoded;
  base::Base64UrlEncode("ref1", base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &ref1_encoded);
  std::string url1 = net::AppendQueryParameter(
                         GURL("https://onetimetoken.pa.googleapis.com/v1/"
                              "onetimetokens:fetchEmail"),
                         "encryptedMessageReference", ref1_encoded)
                         .spec();
  test_url_loader_factory_.AddResponse(url1, "");

  // Completing ref1 should trigger ref4's access token fetch.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 3);  // ref2, ref3, ref4
}

// Tests that duplicate tickles are ignored.
TEST_F(GmailOtpBackendImplTest, DeDuplicatesIncomingTickles) {
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = backend_.Subscribe(
      base::Time::Now() + base::Minutes(1), future.GetRepeatingCallback());

  // Send the same tickle 3 times.
  const OneTimeTokenBackendNotification notification(
      EncryptedMessageReference("duplicate_ref"));
  backend_.OnIncomingOneTimeTokenBackendNotification(notification);
  backend_.OnIncomingOneTimeTokenBackendNotification(notification);
  backend_.OnIncomingOneTimeTokenBackendNotification(notification);

  // Only 1 access token fetch should be started.
  EXPECT_EQ(identity_test_env_.GetPendingAccessTokenRequests().size(), 1u);
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  // Complete the request.
  std::string encoded_ref;
  base::Base64UrlEncode("duplicate_ref",
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_ref);
  std::string url = net::AppendQueryParameter(
                        GURL("https://onetimetoken.pa.googleapis.com/v1/"
                             "onetimetokens:fetchEmail"),
                        "encryptedMessageReference", encoded_ref)
                        .spec();
  test_url_loader_factory_.AddResponse(url, "");
  auto unused = future.Get();
}

// Tests that tickles received just before a subscription are processed when
// the subscription is created.
TEST_F(GmailOtpBackendImplTest, RecentTicklesProcessedUponSubscription) {
  base::HistogramTester histogram_tester;
  // Tickle arrives before anyone is subscribed.
  backend_.OnIncomingOneTimeTokenBackendNotification(
      OneTimeTokenBackendNotification(EncryptedMessageReference("ref1")));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OneTimeTokens.Backend.Gmail.HasActiveSubscription", false, 1);

  // Subscription arrives.
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = backend_.Subscribe(
      base::Time::Now() + base::Minutes(1), future.GetRepeatingCallback());

  // The cached tickle should now trigger a request.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  // Complete to avoid dangling pointers.
  std::string encoded_reference;
  base::Base64UrlEncode("ref1", base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_reference);
  std::string url = net::AppendQueryParameter(
                        GURL("https://onetimetoken.pa.googleapis.com/v1/"
                             "onetimetokens:fetchEmail"),
                        "encryptedMessageReference", encoded_reference)
                        .spec();
  test_url_loader_factory_.AddResponse(url, "");
}

// Tests that expired tickles are not processed upon subscription.
TEST_F(GmailOtpBackendImplTest, ExpiredTicklesNotProcessedUponSubscription) {
  // Tickle arrives.
  backend_.OnIncomingOneTimeTokenBackendNotification(
      OneTimeTokenBackendNotification(EncryptedMessageReference("ref1")));

  // Time passes, tickle expires.
  task_environment_.FastForwardBy(kNotificationExpirationDuration +
                                  base::Seconds(1));

  // Subscription arrives.
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = backend_.Subscribe(
      base::Time::Now() + base::Minutes(1), future.GetRepeatingCallback());

  // No request should be triggered for the expired tickle.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

// Tests that SubscriptionWaitLatency is recorded as 0 when a subscription is
// already active.
TEST_F(GmailOtpBackendImplTest, SubscriptionWaitLatencyIsZeroWhenActive) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = backend_.Subscribe(
      base::Time::Now() + base::Minutes(1), future.GetRepeatingCallback());

  backend_.OnIncomingOneTimeTokenBackendNotification(
      OneTimeTokenBackendNotification(EncryptedMessageReference("ref1")));

  histogram_tester.ExpectTimeBucketCount(
      "Autofill.OneTimeTokens.Backend.Gmail.SubscriptionWaitLatency",
      base::TimeDelta(), 1);
}

// Tests that SubscriptionWaitLatency records the actual wait time when
// processed from cache.
TEST_F(GmailOtpBackendImplTest, SubscriptionWaitLatencyRecordsWaitTime) {
  base::HistogramTester histogram_tester;
  // Tickle arrives before anyone is subscribed.
  backend_.OnIncomingOneTimeTokenBackendNotification(
      OneTimeTokenBackendNotification(EncryptedMessageReference("ref1")));

  // Time passes.
  task_environment_.FastForwardBy(base::Seconds(2));

  // Subscription arrives.
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = backend_.Subscribe(
      base::Time::Now() + base::Minutes(1), future.GetRepeatingCallback());

  histogram_tester.ExpectTimeBucketCount(
      "Autofill.OneTimeTokens.Backend.Gmail.SubscriptionWaitLatency",
      base::Seconds(2), 1);
}

}  // namespace one_time_tokens
