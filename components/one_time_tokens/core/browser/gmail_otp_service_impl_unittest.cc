// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/gmail_otp_service_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/one_time_tokens/core/browser/fake_gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace one_time_tokens {

class GmailOtpServiceImplTest : public testing::Test {
 public:
  GmailOtpServiceImplTest() {
    service_ = std::make_unique<GmailOtpServiceImpl>(
        &backend_, &fake_affiliation_service_, std::nullopt);
  }

  void TearDown() override { service_.reset(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeGmailOtpBackend backend_;
  affiliations::FakeAffiliationService fake_affiliation_service_;
  std::unique_ptr<GmailOtpServiceImpl> service_;
};

TEST_F(GmailOtpServiceImplTest, GetFreshestCachedOtp_OpaqueOrigin) {
  base::test::TestFuture<std::optional<OneTimeToken>> future;
  service_->GetFreshestCachedOtp(url::Origin(), /*allow_psl_matching=*/false,
                                 future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(GmailOtpServiceImplTest, GetFreshestCachedOtp_EmptyCache) {
  base::test::TestFuture<std::optional<OneTimeToken>> future;
  service_->GetFreshestCachedOtp(
      url::Origin::Create(GURL("https://example.com")),
      /*allow_psl_matching=*/false, future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(GmailOtpServiceImplTest, GetFreshestCachedOtp_NoMatch) {
  backend_.ProcessCallbacks(base::ok(
      OneTimeToken(OneTimeTokenType::kGmail, "123456", base::TimeTicks::Now(),
                   "noreply@other.com", base::Time::Now())));

  base::test::TestFuture<std::optional<OneTimeToken>> future;
  service_->GetFreshestCachedOtp(
      url::Origin::Create(GURL("https://example.com")),
      /*allow_psl_matching=*/false, future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(GmailOtpServiceImplTest, GetFreshestCachedOtp_FreshestMatchingReturned) {
  base::Time older_time = base::Time::Now() - base::Seconds(30);
  base::Time newer_time = base::Time::Now() - base::Seconds(10);

  backend_.ProcessCallbacks(base::ok(
      OneTimeToken(OneTimeTokenType::kGmail, "111111", base::TimeTicks::Now(),
                   "noreply@example.com", older_time)));
  backend_.ProcessCallbacks(base::ok(
      OneTimeToken(OneTimeTokenType::kGmail, "222222", base::TimeTicks::Now(),
                   "noreply@example.com", newer_time)));
  backend_.ProcessCallbacks(base::ok(
      OneTimeToken(OneTimeTokenType::kGmail, "333333", base::TimeTicks::Now(),
                   "noreply@different.com", base::Time::Now())));

  base::test::TestFuture<std::optional<OneTimeToken>> future;
  service_->GetFreshestCachedOtp(
      url::Origin::Create(GURL("https://example.com")),
      /*allow_psl_matching=*/false, future.GetCallback());

  std::optional<OneTimeToken> result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->value(), "222222");
}

TEST_F(GmailOtpServiceImplTest,
       GetFreshestCachedOtp_ServiceDestroyedDuringPendingExactMatchCheck) {
  backend_.ProcessCallbacks(base::ok(
      OneTimeToken(OneTimeTokenType::kGmail, "123456", base::TimeTicks::Now(),
                   "noreply@example.com", base::Time::Now())));

  bool callback_called = false;
  service_->GetFreshestCachedOtp(
      url::Origin::Create(GURL("https://example.com")),
      /*allow_psl_matching=*/false,
      base::BindLambdaForTesting(
          [&callback_called](std::optional<OneTimeToken>) {
            callback_called = true;
          }));

  // Destroy `service_` while `DomainRelationChecker::Check`'s posted task is
  // still queued on the task runner; `GmailOtpSenderDomainMatcher` must be
  // destroyed before `service_->domain_relation_checker_`.
  service_.reset();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(callback_called);
}

TEST_F(GmailOtpServiceImplTest,
       GetFreshestCachedOtp_ServiceDestroyedDuringPendingAffiliationCheck) {
  testing::NiceMock<affiliations::MockAffiliationService>
      mock_affiliation_service;
  affiliations::AffiliationService::ResultCallback pending_affiliation_callback;
  EXPECT_CALL(mock_affiliation_service, GetAffiliationsAndBranding)
      .WillOnce([&pending_affiliation_callback](
                    const affiliations::FacetURI&,
                    affiliations::AffiliationService::ResultCallback callback) {
        pending_affiliation_callback = std::move(callback);
      });

  auto service = std::make_unique<GmailOtpServiceImpl>(
      &backend_, &mock_affiliation_service, std::nullopt);
  backend_.ProcessCallbacks(base::ok(
      OneTimeToken(OneTimeTokenType::kGmail, "123456", base::TimeTicks::Now(),
                   "noreply@other.com", base::Time::Now())));

  bool callback_called = false;
  service->GetFreshestCachedOtp(
      url::Origin::Create(GURL("https://example.com")),
      /*allow_psl_matching=*/false,
      base::BindLambdaForTesting(
          [&callback_called](std::optional<OneTimeToken>) {
            callback_called = true;
          }));
  ASSERT_TRUE(pending_affiliation_callback);

  // Destroy `service` before `AffiliationService` finishes or drops its
  // callback (matching `KeyedService` teardown order).
  service.reset();
  std::move(pending_affiliation_callback).Run({}, /*success=*/true);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(callback_called);
}

TEST_F(GmailOtpServiceImplTest, Subscribe_OpaqueOrigin) {
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      valid_future;
  ExpiringSubscription valid_subscription = service_->Subscribe(
      url::Origin::Create(GURL("https://example.com")),
      /*allow_psl_matching=*/false, base::Time::Now() + base::Minutes(1),
      valid_future.GetRepeatingCallback(), base::DoNothing());
  ASSERT_TRUE(valid_subscription.IsAlive());

  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription =
      service_->Subscribe(url::Origin(), /*allow_psl_matching=*/false,
                          base::Time::Now() + base::Minutes(1),
                          future.GetRepeatingCallback(), base::DoNothing());
  EXPECT_TRUE(subscription.IsAlive());
  base::expected<OneTimeToken, OneTimeTokenRetrievalError> result =
      future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OneTimeTokenRetrievalError::kGmailOtpUnknown);
  EXPECT_FALSE(valid_future.IsReady());

  // If a token arrives while the opaque-origin subscription is still alive, it
  // should skip domain matching for the opaque subscriber without crashing.
  backend_.ProcessCallbacks(base::ok(
      OneTimeToken(OneTimeTokenType::kGmail, "654321", base::TimeTicks::Now(),
                   "noreply@example.com", base::Time::Now())));
  base::expected<OneTimeToken, OneTimeTokenRetrievalError> valid_result =
      valid_future.Take();
  ASSERT_TRUE(valid_result.has_value());
  EXPECT_EQ(valid_result->value(), "654321");
  EXPECT_FALSE(future.IsReady());
}

TEST_F(GmailOtpServiceImplTest, Subscribe_ReceivesMatchingToken) {
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = service_->Subscribe(
      url::Origin::Create(GURL("https://example.com")),
      /*allow_psl_matching=*/false, base::Time::Now() + base::Minutes(1),
      future.GetRepeatingCallback(), base::DoNothing());
  ASSERT_TRUE(subscription.IsAlive());

  backend_.ProcessCallbacks(base::ok(
      OneTimeToken(OneTimeTokenType::kGmail, "654321", base::TimeTicks::Now(),
                   "noreply@example.com", base::Time::Now())));

  base::expected<OneTimeToken, OneTimeTokenRetrievalError> result =
      future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->value(), "654321");
}

TEST_F(GmailOtpServiceImplTest, Subscribe_IgnoresNonMatchingToken) {
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = service_->Subscribe(
      url::Origin::Create(GURL("https://example.com")),
      /*allow_psl_matching=*/false, base::Time::Now() + base::Minutes(1),
      future.GetRepeatingCallback(), base::DoNothing());
  ASSERT_TRUE(subscription.IsAlive());

  backend_.ProcessCallbacks(base::ok(
      OneTimeToken(OneTimeTokenType::kGmail, "654321", base::TimeTicks::Now(),
                   "noreply@different.com", base::Time::Now())));

  base::expected<OneTimeToken, OneTimeTokenRetrievalError> result =
      future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            OneTimeTokenRetrievalError::kGmailOtpSenderDomainMismatch);
}

TEST_F(GmailOtpServiceImplTest, Subscribe_DispatchesErrorToSubscribers) {
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = service_->Subscribe(
      url::Origin::Create(GURL("https://example.com")),
      /*allow_psl_matching=*/false, base::Time::Now() + base::Minutes(1),
      future.GetRepeatingCallback(), base::DoNothing());
  ASSERT_TRUE(subscription.IsAlive());

  backend_.ProcessCallbacks(
      base::unexpected(OneTimeTokenRetrievalError::kGmailOtpBackendAuthError));

  base::expected<OneTimeToken, OneTimeTokenRetrievalError> result =
      future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            OneTimeTokenRetrievalError::kGmailOtpBackendAuthError);
}

TEST_F(GmailOtpServiceImplTest, Subscribe_Expiration) {
  base::test::TestFuture<void> expiration_future;
  ExpiringSubscription subscription = service_->Subscribe(
      url::Origin::Create(GURL("https://example.com")),
      /*allow_psl_matching=*/false, base::Time::Now() + base::Seconds(30),
      base::DoNothing(), expiration_future.GetCallback());
  ASSERT_TRUE(subscription.IsAlive());
  EXPECT_EQ(backend_.num_callbacks(), 1u);

  task_environment_.FastForwardBy(base::Seconds(31));
  EXPECT_TRUE(expiration_future.IsReady());
  EXPECT_FALSE(subscription.IsAlive());
  EXPECT_EQ(backend_.num_callbacks(), 0u);
}

TEST_F(GmailOtpServiceImplTest,
       Subscribe_ExtendedExpirationReinstantiatesBackendSubscription) {
  base::Time now = base::Time::Now();
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      token_future;
  base::test::TestFuture<void> expiration_future;
  ExpiringSubscription subscription = service_->Subscribe(
      url::Origin::Create(GURL("https://example.com")),
      /*allow_psl_matching=*/false, now + base::Seconds(30),
      token_future.GetRepeatingCallback(), expiration_future.GetCallback());
  ASSERT_TRUE(subscription.IsAlive());
  EXPECT_EQ(backend_.num_callbacks(), 1u);

  // Client extends its subscription beyond the initial backend subscription
  // expiration.
  subscription.SetExpirationTime(now + base::Seconds(60));

  // Fast-forward past the initial 30s expiration: the backend subscription
  // expires and should be re-instantiated for the remaining subscriber.
  task_environment_.FastForwardBy(base::Seconds(31));
  EXPECT_FALSE(expiration_future.IsReady());
  EXPECT_TRUE(subscription.IsAlive());
  EXPECT_EQ(backend_.num_callbacks(), 1u);

  // Verify the subscriber still receives tokens after backend re-subscription.
  backend_.ProcessCallbacks(base::ok(
      OneTimeToken(OneTimeTokenType::kGmail, "654321", base::TimeTicks::Now(),
                   "noreply@example.com", base::Time::Now())));
  base::expected<OneTimeToken, OneTimeTokenRetrievalError> result =
      token_future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->value(), "654321");

  // Fast-forward past the extended 60s expiration: both the subscriber and
  // backend subscription should now expire.
  task_environment_.FastForwardBy(base::Seconds(30));
  EXPECT_TRUE(expiration_future.IsReady());
  EXPECT_FALSE(subscription.IsAlive());
  EXPECT_EQ(backend_.num_callbacks(), 0u);
}

TEST_F(GmailOtpServiceImplTest, Subscribe_CancelPreventsNotification) {
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  ExpiringSubscription subscription = service_->Subscribe(
      url::Origin::Create(GURL("https://example.com")),
      /*allow_psl_matching=*/false, base::Time::Now() + base::Minutes(1),
      future.GetRepeatingCallback(), base::DoNothing());
  ASSERT_TRUE(subscription.IsAlive());

  subscription.Cancel();
  EXPECT_FALSE(subscription.IsAlive());

  backend_.ProcessCallbacks(base::ok(
      OneTimeToken(OneTimeTokenType::kGmail, "654321", base::TimeTicks::Now(),
                   "noreply@example.com", base::Time::Now())));

  EXPECT_FALSE(future.IsReady());
}

TEST_F(GmailOtpServiceImplTest, Subscribe_DestructionPreventsNotification) {
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  {
    ExpiringSubscription subscription = service_->Subscribe(
        url::Origin::Create(GURL("https://example.com")),
        /*allow_psl_matching=*/false, base::Time::Now() + base::Minutes(1),
        future.GetRepeatingCallback(), base::DoNothing());
    ASSERT_TRUE(subscription.IsAlive());
  }

  backend_.ProcessCallbacks(base::ok(
      OneTimeToken(OneTimeTokenType::kGmail, "654321", base::TimeTicks::Now(),
                   "noreply@example.com", base::Time::Now())));

  EXPECT_FALSE(future.IsReady());
}

TEST_F(GmailOtpServiceImplTest,
       Subscribe_ExpirationCancelsInFlightDomainCheck) {
  testing::NiceMock<affiliations::MockAffiliationService>
      mock_affiliation_service;
  affiliations::AffiliationService::ResultCallback pending_affiliation_callback;
  EXPECT_CALL(mock_affiliation_service, GetAffiliationsAndBranding)
      .WillOnce([&pending_affiliation_callback](
                    const affiliations::FacetURI&,
                    affiliations::AffiliationService::ResultCallback callback) {
        pending_affiliation_callback = std::move(callback);
      });

  auto service = std::make_unique<GmailOtpServiceImpl>(
      &backend_, &mock_affiliation_service, std::nullopt);
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      token_future;
  base::test::TestFuture<void> expiration_future;
  ExpiringSubscription subscription = service->Subscribe(
      url::Origin::Create(GURL("https://example.com")),
      /*allow_psl_matching=*/false, base::Time::Now() + base::Seconds(30),
      token_future.GetRepeatingCallback(), expiration_future.GetCallback());

  backend_.ProcessCallbacks(base::ok(
      OneTimeToken(OneTimeTokenType::kGmail, "654321", base::TimeTicks::Now(),
                   "noreply@other.com", base::Time::Now())));
  ASSERT_TRUE(pending_affiliation_callback);
  EXPECT_TRUE(service->HasPendingRequests(subscription));

  task_environment_.FastForwardBy(base::Seconds(31));
  EXPECT_TRUE(expiration_future.IsReady());
  EXPECT_FALSE(service->HasPendingRequests(subscription));

  std::move(pending_affiliation_callback).Run({}, /*success=*/true);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(token_future.IsReady());
}

TEST_F(GmailOtpServiceImplTest,
       Subscribe_ServiceDestroyedDuringPendingAffiliationCheck) {
  testing::NiceMock<affiliations::MockAffiliationService>
      mock_affiliation_service;
  affiliations::AffiliationService::ResultCallback pending_affiliation_callback;
  EXPECT_CALL(mock_affiliation_service, GetAffiliationsAndBranding)
      .WillOnce([&pending_affiliation_callback](
                    const affiliations::FacetURI&,
                    affiliations::AffiliationService::ResultCallback callback) {
        pending_affiliation_callback = std::move(callback);
      });

  auto service = std::make_unique<GmailOtpServiceImpl>(
      &backend_, &mock_affiliation_service, std::nullopt);
  bool callback_called = false;
  ExpiringSubscription subscription = service->Subscribe(
      url::Origin::Create(GURL("https://example.com")),
      /*allow_psl_matching=*/false, base::Time::Now() + base::Minutes(1),
      base::BindLambdaForTesting(
          [&callback_called](
              base::expected<OneTimeToken, OneTimeTokenRetrievalError>) {
            callback_called = true;
          }),
      base::DoNothing());

  backend_.ProcessCallbacks(base::ok(
      OneTimeToken(OneTimeTokenType::kGmail, "654321", base::TimeTicks::Now(),
                   "noreply@other.com", base::Time::Now())));
  ASSERT_TRUE(pending_affiliation_callback);

  service.reset();
  std::move(pending_affiliation_callback).Run({}, /*success=*/true);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(callback_called);
}

TEST_F(GmailOtpServiceImplTest, HasPendingRequestsAndGetLogSink) {
  EXPECT_EQ(service_->GetLogSink(), backend_.GetLogSink());

  // Subscribe two subscribers and verify that when the first subscriber's
  // domain check finishes, HasPendingRequests() is false for that subscriber
  // even while the other subscriber's domain check is still pending.
  base::test::TestFuture<void> sub1_done;
  base::test::TestFuture<void> sub2_done;
  ExpiringSubscription sub1;
  ExpiringSubscription sub2;
  bool sub1_self_pending = true;
  bool sub2_self_pending = true;
  bool other_pending_when_first_completed = false;
  int completed_count = 0;

  sub1 = service_->Subscribe(
      url::Origin::Create(GURL("https://example.com")),
      /*allow_psl_matching=*/false, base::Time::Now() + base::Minutes(1),
      base::BindLambdaForTesting(
          [&](base::expected<OneTimeToken, OneTimeTokenRetrievalError>) {
            sub1_self_pending = service_->HasPendingRequests(sub1);
            if (++completed_count == 1) {
              other_pending_when_first_completed =
                  service_->HasPendingRequests(sub2);
            }
            sub1_done.SetValue();
          }),
      base::DoNothing());
  sub2 = service_->Subscribe(
      url::Origin::Create(GURL("https://example.com")),
      /*allow_psl_matching=*/false, base::Time::Now() + base::Minutes(1),
      base::BindLambdaForTesting(
          [&](base::expected<OneTimeToken, OneTimeTokenRetrievalError>) {
            sub2_self_pending = service_->HasPendingRequests(sub2);
            if (++completed_count == 1) {
              other_pending_when_first_completed =
                  service_->HasPendingRequests(sub1);
            }
            sub2_done.SetValue();
          }),
      base::DoNothing());

  EXPECT_FALSE(service_->HasPendingRequests(sub1));
  backend_.SetHasPendingRequests(true);
  EXPECT_TRUE(service_->HasPendingRequests(sub1));
  backend_.SetHasPendingRequests(false);
  EXPECT_FALSE(service_->HasPendingRequests(sub1));

  backend_.ProcessCallbacks(base::ok(
      OneTimeToken(OneTimeTokenType::kGmail, "654321", base::TimeTicks::Now(),
                   "noreply@example.com", base::Time::Now())));
  EXPECT_TRUE(service_->HasPendingRequests(sub1));
  EXPECT_TRUE(service_->HasPendingRequests(sub2));

  ASSERT_TRUE(sub1_done.Wait());
  ASSERT_TRUE(sub2_done.Wait());
  EXPECT_FALSE(sub1_self_pending);
  EXPECT_FALSE(sub2_self_pending);
  EXPECT_TRUE(other_pending_when_first_completed);
  EXPECT_FALSE(service_->HasPendingRequests(sub1));
  EXPECT_FALSE(service_->HasPendingRequests(sub2));

  // Cancelling a subscription while a domain check is in flight immediately
  // clears HasPendingRequests() for that subscription.
  sub1.Cancel();
  sub2_done.Clear();
  backend_.ProcessCallbacks(base::ok(
      OneTimeToken(OneTimeTokenType::kGmail, "999999", base::TimeTicks::Now(),
                   "noreply@example.com", base::Time::Now())));
  EXPECT_TRUE(service_->HasPendingRequests(sub2));
  sub2.Cancel();
  EXPECT_FALSE(service_->HasPendingRequests(sub2));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(sub2_done.IsReady());
}

}  // namespace one_time_tokens
