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
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/one_time_tokens/core/browser/fetch_email_one_time_token_response.pb.h"
#include "components/one_time_tokens/core/browser/fetch_user_data_processing_consent_response.pb.h"
#include "components/one_time_tokens/core/browser/one_time_token_service_constants.h"
#include "components/one_time_tokens/core/browser/user_data_processing_consent_states.h"
#include "components/one_time_tokens/core/common/one_time_token_features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

using ::google::internal::chrome::passwords::onetimetoken::v1::
    FetchEmailOneTimeTokenResponse;
using ::google::internal::chrome::passwords::onetimetoken::v1::
    FetchUserDataProcessingConsentResponse;
using ::google::internal::chrome::passwords::onetimetoken::v1::
    USER_DATA_PROCESSING_CONSENT_STATE_DISABLED;
using ::google::internal::chrome::passwords::onetimetoken::v1::
    USER_DATA_PROCESSING_CONSENT_STATE_ENABLED;

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
  std::string GetExpectedUrl(const std::string& unencoded_reference) {
    std::string encoded_reference;
    base::Base64UrlEncode(unencoded_reference,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &encoded_reference);
    GURL url = net::AppendQueryParameter(
        GURL("https://onetimetoken.pa.googleapis.com/v1/"
             "onetimetokens:fetchEmail"),
        "encryptedMessageReference", encoded_reference);
    return net::AppendQueryParameter(url, "alt", "proto").spec();
  }

  std::string GetExpectedConsentUrl() {
    GURL url(
        "https://onetimetoken.pa.googleapis.com/v1/"
        "onetimetokens:fetchUserDataProcessingConsent");
    return net::AppendQueryParameter(url, "alt", "proto").spec();
  }

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

  FetchEmailOneTimeTokenResponse response;
  response.mutable_one_time_password()->set_one_time_password("123456");
  response.set_sender_address("noreply@example.com");

  task_environment_.FastForwardBy(base::Milliseconds(500));

  test_url_loader_factory_.AddResponse(
      GetExpectedUrl(/*unencoded_reference=*/"encrypted_reference"),
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

  task_environment_.FastForwardBy(base::Milliseconds(500));

  // Return an HTTP 500 to simulate a network error
  test_url_loader_factory_.AddResponse(
      GetExpectedUrl(/*unencoded_reference=*/"encrypted_reference_fail"), "",
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
  test_url_loader_factory_.AddResponse(
      GetExpectedUrl(/*unencoded_reference=*/"ref1"), "");
  test_url_loader_factory_.AddResponse(
      GetExpectedUrl(/*unencoded_reference=*/"ref2"), "");
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
  test_url_loader_factory_.AddResponse(
      GetExpectedUrl(/*unencoded_reference=*/"ref1"), "");

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
  test_url_loader_factory_.AddResponse(
      GetExpectedUrl(/*unencoded_reference=*/"duplicate_ref"), "");
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
  test_url_loader_factory_.AddResponse(
      GetExpectedUrl(/*unencoded_reference=*/"ref1"), "");
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

// Tests that if the backend is created with null URLLoaderFactory, Subscribe
// immediately fails with kGmailOtpBackendInitializationFailed.
TEST_F(GmailOtpBackendImplTest, Subscribe_InitializationFailed) {
  base::HistogramTester histogram_tester;
  GmailOtpBackendImpl bad_backend(/*url_loader_factory=*/nullptr,
                                  *identity_test_env_.identity_manager());
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = bad_backend.Subscribe(
      base::Time::Now() + base::Minutes(1), future.GetRepeatingCallback());

  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>& result =
      future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            OneTimeTokenRetrievalError::kGmailOtpBackendInitializationFailed);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OneTimeTokens.Backend.Gmail.Success", false, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OneTimeTokens.Backend.Gmail.ErrorCode",
      static_cast<int>(
          OneTimeTokenRetrievalError::kGmailOtpBackendInitializationFailed),
      1);
}

TEST_F(GmailOtpBackendImplTest, FetchUserDataProcessingConsent_Success) {
  base::test::TestFuture<std::optional<UserDataProcessingConsentStates>> future;
  backend_.FetchUserDataProcessingConsent(future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  FetchUserDataProcessingConsentResponse response;
  response.set_comms_apps(USER_DATA_PROCESSING_CONSENT_STATE_ENABLED);
  response.set_google_apps(USER_DATA_PROCESSING_CONSENT_STATE_DISABLED);

  test_url_loader_factory_.AddResponse(GetExpectedConsentUrl(),
                                       response.SerializeAsString());

  std::optional<UserDataProcessingConsentStates> result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->comms_apps, ConsentState::kEnabled);
  EXPECT_EQ(result->google_apps, ConsentState::kDisabled);
}

TEST_F(GmailOtpBackendImplTest, FetchUserDataProcessingConsent_NetworkError) {
  base::test::TestFuture<std::optional<UserDataProcessingConsentStates>> future;
  backend_.FetchUserDataProcessingConsent(future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  test_url_loader_factory_.AddResponse(GetExpectedConsentUrl(), "",
                                       net::HTTP_INTERNAL_SERVER_ERROR);

  std::optional<UserDataProcessingConsentStates> result = future.Get();
  EXPECT_FALSE(result.has_value());
}

TEST_F(GmailOtpBackendImplTest, FetchUserDataProcessingConsent_AuthError) {
  base::test::TestFuture<std::optional<UserDataProcessingConsentStates>> future;
  backend_.FetchUserDataProcessingConsent(future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED));

  std::optional<UserDataProcessingConsentStates> result = future.Get();
  EXPECT_FALSE(result.has_value());
}

TEST_F(GmailOtpBackendImplTest,
       FetchUserDataProcessingConsent_ConcurrentRequests) {
  base::test::TestFuture<std::optional<UserDataProcessingConsentStates>>
      future_1;
  base::test::TestFuture<std::optional<UserDataProcessingConsentStates>>
      future_2;

  backend_.FetchUserDataProcessingConsent(future_1.GetCallback());
  backend_.FetchUserDataProcessingConsent(future_2.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  FetchUserDataProcessingConsentResponse response;
  response.set_comms_apps(USER_DATA_PROCESSING_CONSENT_STATE_ENABLED);
  response.set_google_apps(USER_DATA_PROCESSING_CONSENT_STATE_DISABLED);

  test_url_loader_factory_.AddResponse(GetExpectedConsentUrl(),
                                       response.SerializeAsString());

  std::optional<UserDataProcessingConsentStates> result_1 = future_1.Get();
  std::optional<UserDataProcessingConsentStates> result_2 = future_2.Get();

  ASSERT_TRUE(result_1.has_value());
  EXPECT_EQ(result_1->comms_apps, ConsentState::kEnabled);
  EXPECT_EQ(result_1->google_apps, ConsentState::kDisabled);

  ASSERT_TRUE(result_2.has_value());
  EXPECT_EQ(result_2->comms_apps, ConsentState::kEnabled);
  EXPECT_EQ(result_2->google_apps, ConsentState::kDisabled);

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
}

TEST_F(GmailOtpBackendImplTest, SubscribeToTicklesAndReceiveNotification) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> future;
  ExpiringSubscription subscription = backend_.SubscribeToTickles(
      base::Time::Now() + base::Minutes(1), future.GetRepeatingCallback());

  backend_.OnIncomingOneTimeTokenBackendNotification(
      OneTimeTokenBackendNotification(
          EncryptedMessageReference("encrypted_reference")));

  EXPECT_TRUE(future.WaitAndClear());
  // Ensure no network requests were made for payload fetching.
  EXPECT_EQ(test_url_loader_factory_.total_requests(), 0u);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OneTimeTokens.Backend.Gmail.HasActiveSubscription", true, 1);
}

TEST_F(GmailOtpBackendImplTest, SubscribeToTicklesWithPreCachedNotification) {
  base::HistogramTester histogram_tester;
  // Push arrives BEFORE subscription.
  backend_.OnIncomingOneTimeTokenBackendNotification(
      OneTimeTokenBackendNotification(
          EncryptedMessageReference("encrypted_reference")));

  histogram_tester.ExpectUniqueSample(
      "Autofill.OneTimeTokens.Backend.Gmail.HasActiveSubscription", false, 1);

  // Now subscribe to tickles: it should immediately fire for the cached
  // notification.
  base::test::TestFuture<void> future;
  ExpiringSubscription subscription = backend_.SubscribeToTickles(
      base::Time::Now() + base::Minutes(1), future.GetRepeatingCallback());

  EXPECT_TRUE(future.WaitAndClear());
  EXPECT_EQ(test_url_loader_factory_.total_requests(), 0u);
}

TEST_F(GmailOtpBackendImplTest, SubscribeToTicklesExpiration) {
  base::test::TestFuture<void> future;
  ExpiringSubscription subscription = backend_.SubscribeToTickles(
      base::Time::Now() + base::Seconds(30), future.GetRepeatingCallback());

  EXPECT_TRUE(subscription.IsAlive());
  task_environment_.FastForwardBy(base::Seconds(31));
  EXPECT_FALSE(subscription.IsAlive());

  // Further notifications should not reach the expired subscription.
  backend_.OnIncomingOneTimeTokenBackendNotification(
      OneTimeTokenBackendNotification(
          EncryptedMessageReference("encrypted_reference")));
  EXPECT_FALSE(future.IsReady());
}

TEST_F(GmailOtpBackendImplTest, HasPendingRequests) {
  EXPECT_FALSE(backend_.HasPendingRequests());

  // Incoming notification adds to notification_cache_, HasPendingRequests is
  // true.
  backend_.OnIncomingOneTimeTokenBackendNotification(
      OneTimeTokenBackendNotification(
          EncryptedMessageReference("encrypted_reference")));
  EXPECT_TRUE(backend_.HasPendingRequests());

  FetchEmailOneTimeTokenResponse response;
  response.mutable_one_time_password()->set_one_time_password("123456");
  response.set_sender_address("noreply@example.com");
  test_url_loader_factory_.AddResponse(
      GetExpectedUrl(/*unencoded_reference=*/"encrypted_reference"),
      response.SerializeAsString());

  // Subscribe starts the fetch; HasPendingRequests is still true.
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = backend_.Subscribe(
      base::Time::Now() + base::Minutes(1), future.GetRepeatingCallback());
  EXPECT_TRUE(backend_.HasPendingRequests());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_FALSE(backend_.HasPendingRequests());
}

TEST_F(GmailOtpBackendImplTest,
       ProcessCachedNotifications_ProcessesNewestFirst) {
  backend_.OnIncomingOneTimeTokenBackendNotification(
      OneTimeTokenBackendNotification(
          EncryptedMessageReference("old_reference")));
  task_environment_.FastForwardBy(base::Seconds(1));
  backend_.OnIncomingOneTimeTokenBackendNotification(
      OneTimeTokenBackendNotification(
          EncryptedMessageReference("new_reference")));

  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = backend_.Subscribe(
      base::Time::Now() + base::Minutes(1), future.GetRepeatingCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 2);
  EXPECT_EQ(test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
            GetExpectedUrl(/*unencoded_reference=*/"new_reference"));
  EXPECT_EQ(test_url_loader_factory_.GetPendingRequest(1)->request.url.spec(),
            GetExpectedUrl(/*unencoded_reference=*/"old_reference"));

  // Complete requests to avoid dangling pointers.
  test_url_loader_factory_.AddResponse(
      GetExpectedUrl(/*unencoded_reference=*/"new_reference"), "");
  test_url_loader_factory_.AddResponse(
      GetExpectedUrl(/*unencoded_reference=*/"old_reference"), "");
}

TEST_F(GmailOtpBackendImplTest, ExpiredOnArrivalLogsTickleArrivalMetric) {
  base::test::ScopedFeatureList feature_list{
      features::kGmailOtpRetrievalService};
  base::HistogramTester histogram_tester;
  base::TimeTicks old_timestamp = base::TimeTicks::Now() -
                                  kNotificationExpirationDuration -
                                  base::Seconds(1);
  OneTimeTokenBackendNotification notification(
      EncryptedMessageReference("ref1"),
      /*otp_created_timestamp=*/base::Time::Now(),
      /*email_received_timestamp=*/base::Time::Now(),
      /*notification_sent_timestamp=*/base::Time::Now(),
      /*notification_received_timestamp=*/base::Time::Now(),
      /*notification_received_timeticks=*/old_timestamp);

  backend_.OnIncomingOneTimeTokenBackendNotification(notification);

  histogram_tester.ExpectUniqueSample(kTickleArrivalHistogram,
                                      TickleArrival::kExpiredOnArrival, 1);
}

TEST_F(GmailOtpBackendImplTest, ExpiredOnArrival_NotLoggedIfFeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      features::kGmailOtpRetrievalService);
  base::HistogramTester histogram_tester;
  base::TimeTicks old_timestamp = base::TimeTicks::Now() -
                                  kNotificationExpirationDuration -
                                  base::Seconds(1);
  OneTimeTokenBackendNotification notification(
      EncryptedMessageReference("ref1"),
      /*otp_created_timestamp=*/base::Time::Now(),
      /*email_received_timestamp=*/base::Time::Now(),
      /*notification_sent_timestamp=*/base::Time::Now(),
      /*notification_received_timestamp=*/base::Time::Now(),
      /*notification_received_timeticks=*/old_timestamp);

  backend_.OnIncomingOneTimeTokenBackendNotification(notification);

  histogram_tester.ExpectTotalCount(kTickleArrivalHistogram, 0);
}

}  // namespace one_time_tokens
