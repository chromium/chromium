// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/sms_otp_service_impl.h"

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"
#include "components/one_time_tokens/core/browser/sms_otp_backend.h"
#include "components/one_time_tokens/core/common/one_time_token_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::Optional;
using ::testing::Property;
using ::testing::SaveArg;

MATCHER_P(OneTimeTokenValueEq, expected_token_value, "") {
  if (!arg.has_value()) {
    *result_listener << "Expected OneTimeToken, but got error.";
    return false;
  }
  const OneTimeToken& actual_token = arg.value();
  if (actual_token.value() != expected_token_value) {
    *result_listener << "Token value mismatch: expected "
                     << expected_token_value << ", got "
                     << actual_token.value();
    return false;
  }
  return true;
}

class MockSmsOtpBackend : public SmsOtpBackend {
 public:
  void RetrieveSmsOtp(
      base::OnceCallback<void(
          base::expected<OneTimeToken, OneTimeTokenRetrievalError>)> callback)
      override {
    callbacks_.push_back(std::move(callback));
    RetrieveSmsOtpCalled();
  }

  bool HasPendingRetrieveSmsOtpCallbacks() { return !callbacks_.empty(); }

  void SimulateOtpArrived(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply) {
    for (auto& callback : callbacks_) {
      std::move(callback).Run(reply);
    }
    callbacks_.clear();
  }

  // Use this method for expectations on how many times `RetrieveSmsOtp` is
  // called. Note that the `kSmsRefetchInterval` constant determines the time
  // when the next `RetrieveSmsOtpCalled` is scheduled after the previous one
  // returned.
  MOCK_METHOD((void), RetrieveSmsOtpCalled, ());

 private:
  std::list<base::OnceCallback<void(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>>
      callbacks_;
};

// A helper class to collect results from the SmsOtpService callbacks.
class SmsOtpServiceTestObserver {
 public:
  void OnTokenReceived(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError> result) {
    results_.push_back(std::move(result));
  }

  const std::vector<base::expected<OneTimeToken, OneTimeTokenRetrievalError>>&
  results() const {
    return results_;
  }

 private:
  std::vector<base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      results_;
};

}  // namespace

class SmsOtpServiceImplTest : public testing::Test {
 public:
  SmsOtpServiceImplTest() = default;
  ~SmsOtpServiceImplTest() override = default;

  void TearDown() override { feature_list_.Reset(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  MockSmsOtpBackend sms_otp_backend_;
};

// Test that with no backend, nothing happens.
TEST_F(SmsOtpServiceImplTest, NoBackend) {
  SmsOtpServiceImpl service(/*sms_otp_backend=*/nullptr);
  SmsOtpServiceTestObserver observer;

  // GetRecentOneTimeTokens should not call the callback.
  service.GetRecentOneTimeTokens(
      base::BindRepeating(&SmsOtpServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)));
  EXPECT_TRUE(observer.results().empty());

  // Subscribe should notify that the platform is not supported.
  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&SmsOtpServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)),
      /*expiration_callback=*/base::DoNothing());
  EXPECT_FALSE(subscription.IsAlive());
  EXPECT_THAT(
      observer.results(),
      ElementsAre(base::test::ErrorIs(
          OneTimeTokenRetrievalError::kSmsOtpBackendPlatformNotSupported)));
}

// Test that subscribing triggers a fetch.
TEST_F(SmsOtpServiceImplTest, SubscriptionTriggersFetch) {
  SmsOtpServiceImpl service(&sms_otp_backend_);
  SmsOtpServiceTestObserver observer;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&SmsOtpServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)),
      /*expiration_callback=*/base::DoNothing());

  ASSERT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
}

// Test that a successful fetch notifies subscribers.
TEST_F(SmsOtpServiceImplTest, SuccessfulFetchNotifiesSubscribers) {
  SmsOtpServiceImpl service(&sms_otp_backend_);
  SmsOtpServiceTestObserver observer;

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&SmsOtpServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)),
      /*expiration_callback=*/base::DoNothing());
  sms_otp_backend_.SimulateOtpArrived(OneTimeToken(
      OneTimeTokenType::kSmsOtp, "123456", base::TimeTicks::Now()));

  ASSERT_THAT(observer.results(), ElementsAre(OneTimeTokenValueEq("123456")));
}

// Test that after a successful fetch the backend is queried again.
TEST_F(SmsOtpServiceImplTest, BackendIsQueriedForFreshTokens) {
  SmsOtpServiceImpl service(&sms_otp_backend_);
  SmsOtpServiceTestObserver observer;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);
  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&SmsOtpServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)),
      /*expiration_callback=*/base::DoNothing());
  sms_otp_backend_.SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "1", base::TimeTicks::Now()));
  Mock::VerifyAndClearExpectations(&sms_otp_backend_);

  // After a few seconds, the backend should be queried a second time.
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);
  task_environment_.FastForwardBy(kSmsRefetchInterval);
  Mock::VerifyAndClearExpectations(&sms_otp_backend_);
  sms_otp_backend_.SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "2", base::TimeTicks::Now()));

  ASSERT_THAT(observer.results(),
              ElementsAre(OneTimeTokenValueEq("1"), OneTimeTokenValueEq("2")));
}

// Test that multiple subscriptions only trigger one fetch.
TEST_F(SmsOtpServiceImplTest, MultipleSubscriptionsOneFetch) {
  SmsOtpServiceImpl service(&sms_otp_backend_);
  SmsOtpServiceTestObserver observer1;
  SmsOtpServiceTestObserver observer2;
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);

  auto subscription1 = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&SmsOtpServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer1)),
      /*expiration_callback=*/base::DoNothing());
  // While the first request is in progress, a second subscription should not
  // trigger a new request.
  auto subscription2 = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&SmsOtpServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer2)),
      /*expiration_callback=*/base::DoNothing());

  ASSERT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  sms_otp_backend_.SimulateOtpArrived(OneTimeToken(
      OneTimeTokenType::kSmsOtp, "123456", base::TimeTicks::Now()));

  ASSERT_EQ(observer1.results().size(), 1u);
  ASSERT_EQ(observer2.results().size(), 1u);
}

// Test that an expired subscription does not receive notifications.
TEST_F(SmsOtpServiceImplTest, ExpiredSubscription) {
  SmsOtpServiceImpl service(&sms_otp_backend_);
  SmsOtpServiceTestObserver observer;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&SmsOtpServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)),
      /*expiration_callback=*/base::DoNothing());

  task_environment_.FastForwardBy(base::Minutes(6));

  ASSERT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  sms_otp_backend_.SimulateOtpArrived(OneTimeToken(
      OneTimeTokenType::kSmsOtp, "123456", base::TimeTicks::Now()));

  EXPECT_TRUE(observer.results().empty());
}

// Test that after all subscriptions expire, a new subscription triggers a new
// fetch.
TEST_F(SmsOtpServiceImplTest, NewSubscriptionAfterAllExpired) {
  SmsOtpServiceImpl service(&sms_otp_backend_);

  SmsOtpServiceTestObserver observer1;
  auto subscription1 = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&SmsOtpServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer1)),
      /*expiration_callback=*/base::DoNothing());
  sms_otp_backend_.SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "1", base::TimeTicks::Now()));
  ASSERT_THAT(observer1.results(), ElementsAre(OneTimeTokenValueEq("1")));

  // Expire the subscription.
  task_environment_.FastForwardBy(base::Minutes(6));

  // Terminate the last callback to the backend.
  sms_otp_backend_.SimulateOtpArrived(
      base::unexpected(OneTimeTokenRetrievalError::kSmsOtpBackendError));
  EXPECT_FALSE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());

  SmsOtpServiceTestObserver observer2;
  auto subscription2 = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&SmsOtpServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer2)),
      /*expiration_callback=*/base::DoNothing());
  EXPECT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  sms_otp_backend_.SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "2", base::TimeTicks::Now()));
  ASSERT_THAT(observer2.results(), ElementsAre(OneTimeTokenValueEq("2")));
}

// Test GetRecentOneTimeTokens returns cached tokens.
TEST_F(SmsOtpServiceImplTest, GetRecentOneTimeTokens) {
  SmsOtpServiceImpl service(&sms_otp_backend_);
  SmsOtpServiceTestObserver subscriber_observer;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&SmsOtpServiceTestObserver::OnTokenReceived,
                          base::Unretained(&subscriber_observer)),
      /*expiration_callback=*/base::DoNothing());

  ASSERT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  sms_otp_backend_.SimulateOtpArrived(OneTimeToken(
      OneTimeTokenType::kSmsOtp, "123456", base::TimeTicks::Now()));

  ASSERT_THAT(subscriber_observer.results(),
              ElementsAre(OneTimeTokenValueEq("123456")));

  // Now, get recent tokens.
  SmsOtpServiceTestObserver recent_observer;
  service.GetRecentOneTimeTokens(
      base::BindRepeating(&SmsOtpServiceTestObserver::OnTokenReceived,
                          base::Unretained(&recent_observer)));
  EXPECT_THAT(recent_observer.results(),
              ElementsAre(OneTimeTokenValueEq("123456")));
}

// Test GetRecentOneTimeTokens does not return expired tokens.
TEST_F(SmsOtpServiceImplTest, GetRecentOneTimeTokens_Expired) {
  SmsOtpServiceImpl service(&sms_otp_backend_);
  SmsOtpServiceTestObserver subscriber_observer;

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&SmsOtpServiceTestObserver::OnTokenReceived,
                          base::Unretained(&subscriber_observer)),
      /*expiration_callback=*/base::DoNothing());

  ASSERT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  sms_otp_backend_.SimulateOtpArrived(OneTimeToken(
      OneTimeTokenType::kSmsOtp, "123456", base::TimeTicks::Now()));

  ASSERT_EQ(subscriber_observer.results().size(), 1u);

  // Expire the token.
  task_environment_.FastForwardBy(base::Minutes(6));

  // Now, get recent tokens.
  SmsOtpServiceTestObserver recent_observer;
  service.GetRecentOneTimeTokens(
      base::BindRepeating(&SmsOtpServiceTestObserver::OnTokenReceived,
                          base::Unretained(&recent_observer)));

  EXPECT_TRUE(recent_observer.results().empty());
}

// Test that an error from the backend is propagated to subscribers.
TEST_F(SmsOtpServiceImplTest, NewFetchAfterCompletion) {
  SmsOtpServiceImpl service(&sms_otp_backend_);
  SmsOtpServiceTestObserver observer;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&SmsOtpServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)),
      /*expiration_callback=*/base::DoNothing());

  ASSERT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  sms_otp_backend_.SimulateOtpArrived(
      base::unexpected(OneTimeTokenRetrievalError::kSmsOtpBackendError));

  ASSERT_EQ(observer.results().size(), 1u);
  const auto& result = observer.results()[0];
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OneTimeTokenRetrievalError::kSmsOtpBackendError);
}

// Test GetCachedOneTimeTokens returns cached tokens, including expired ones.
TEST_F(SmsOtpServiceImplTest, GetCachedOneTimeTokens_ReturnsCachedTokens) {
  SmsOtpServiceImpl service(&sms_otp_backend_);
  SmsOtpServiceTestObserver subscriber_observer;

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&SmsOtpServiceTestObserver::OnTokenReceived,
                          base::Unretained(&subscriber_observer)),
      /*expiration_callback=*/base::DoNothing());

  sms_otp_backend_.SimulateOtpArrived(OneTimeToken(
      OneTimeTokenType::kSmsOtp, "123456", base::TimeTicks::Now()));

  // Verify that the token is returned by GetCachedOneTimeTokens.
  std::vector<OneTimeToken> cached_tokens = service.GetCachedOneTimeTokens();
  ASSERT_EQ(cached_tokens.size(), 1u);
  EXPECT_EQ(cached_tokens[0].value(), "123456");

  // Expire the token.
  task_environment_.FastForwardBy(base::Minutes(6));

  // Verify that the expired token is still returned by GetCachedOneTimeTokens.
  cached_tokens = service.GetCachedOneTimeTokens();
  ASSERT_EQ(cached_tokens.size(), 1u);
  EXPECT_EQ(cached_tokens[0].value(), "123456");
}

// Test GetRecentOneTimeTokens returns the most recent token first.
TEST_F(SmsOtpServiceImplTest, GetRecentOneTimeTokens_MostRecentFirst) {
  SmsOtpServiceImpl service(&sms_otp_backend_);
  SmsOtpServiceTestObserver subscriber_observer;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(AtLeast(1));

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&SmsOtpServiceTestObserver::OnTokenReceived,
                          base::Unretained(&subscriber_observer)),
      /*expiration_callback=*/base::DoNothing());

  // First, an SMS OTP arrives at T = 0.
  sms_otp_backend_.SimulateOtpArrived(OneTimeToken(
      OneTimeTokenType::kSmsOtp, "111111", base::TimeTicks::Now()));

  // 10 seconds later, the backend has been queried again and another SMS OTP
  // arrives at T = 10s.
  task_environment_.FastForwardBy(base::Seconds(10));
  sms_otp_backend_.SimulateOtpArrived(OneTimeToken(
      OneTimeTokenType::kSmsOtp, "222222", base::TimeTicks::Now()));

  // GetRecentOneTimeTokens should return both, with the newer token first.
  SmsOtpServiceTestObserver recent_observer;
  service.GetRecentOneTimeTokens(
      base::BindRepeating(&SmsOtpServiceTestObserver::OnTokenReceived,
                          base::Unretained(&recent_observer)));
  EXPECT_THAT(recent_observer.results(),
              ElementsAre(OneTimeTokenValueEq("222222"),
                          OneTimeTokenValueEq("111111")));
}

// Test that RequestOneTimeToken returns a token when the backend succeeds.
TEST_F(SmsOtpServiceImplTest, RequestOneTimeToken_Success) {
  SmsOtpServiceImpl service(&sms_otp_backend_);
  base::MockOnceCallback<void(std::optional<OneTimeToken>)> callback;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);
  service.RequestOneTimeToken(base::Seconds(1), callback.Get());

  EXPECT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  EXPECT_CALL(callback,
              Run(Optional(Property(&OneTimeToken::value, Eq("123456")))));
  sms_otp_backend_.SimulateOtpArrived(OneTimeToken(
      OneTimeTokenType::kSmsOtp, "123456", base::TimeTicks::Now()));
}

// Test that RequestOneTimeToken returns nullopt when the backend fails.
TEST_F(SmsOtpServiceImplTest, RequestOneTimeToken_Failure) {
  SmsOtpServiceImpl service(&sms_otp_backend_);
  base::MockOnceCallback<void(std::optional<OneTimeToken>)> callback;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);
  service.RequestOneTimeToken(base::Seconds(1), callback.Get());

  EXPECT_CALL(callback, Run(Eq(std::nullopt)));
  sms_otp_backend_.SimulateOtpArrived(
      base::unexpected(OneTimeTokenRetrievalError::kSmsOtpBackendError));
}

// Test that RequestOneTimeToken returns nullopt when there is no backend.
TEST_F(SmsOtpServiceImplTest, RequestOneTimeToken_NoBackend) {
  SmsOtpServiceImpl service(/*sms_otp_backend=*/nullptr);
  base::MockOnceCallback<void(std::optional<OneTimeToken>)> callback;

  EXPECT_CALL(callback, Run(Eq(std::nullopt)));
  service.RequestOneTimeToken(base::Seconds(1), callback.Get());
}

// Test that RequestOneTimeToken returns nullopt when the timeout is reached.
TEST_F(SmsOtpServiceImplTest, RequestOneTimeToken_Timeout) {
  SmsOtpServiceImpl service(&sms_otp_backend_);
  base::MockOnceCallback<void(std::optional<OneTimeToken>)> callback;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);
  service.RequestOneTimeToken(base::Milliseconds(100), callback.Get());

  EXPECT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  EXPECT_CALL(callback, Run(Eq(std::nullopt)));
  task_environment_.FastForwardBy(base::Milliseconds(101));
}

// Test that if the backend responds after the timeout, it doesn't crash and the
// second response is ignored.
TEST_F(SmsOtpServiceImplTest, RequestOneTimeToken_LateResponse) {
  SmsOtpServiceImpl service(&sms_otp_backend_);
  base::MockOnceCallback<void(std::optional<OneTimeToken>)> callback;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);
  service.RequestOneTimeToken(base::Milliseconds(100), callback.Get());

  // Fast forward past the timeout.
  EXPECT_CALL(callback, Run(Eq(std::nullopt)));
  task_environment_.FastForwardBy(base::Milliseconds(101));

  // Now simulate a late response from the backend.
  // This should not call the callback again and should not crash.
  sms_otp_backend_.SimulateOtpArrived(OneTimeToken(
      OneTimeTokenType::kSmsOtp, "123456", base::TimeTicks::Now()));
}

// Tests that a `LogHandler` passed to `SmsOtpServiceImpl` is registered
// with `log_sink()` for the lifetime of the service.
TEST_F(SmsOtpServiceImplTest, LogHandlerRegisteredInConstructor) {
  base::test::TestFuture<std::string> log_future;
  SmsOtpServiceImpl service(
      /*sms_otp_backend=*/nullptr,
      base::BindRepeating(
          [](base::RepeatingCallback<void(std::string)> cb,
             std::string_view message) { cb.Run(std::string(message)); },
          log_future.GetRepeatingCallback()));

  EXPECT_TRUE(service.log_sink()->IsActive());
  LOG_OTT(service.log_sink()) << "test log message";
  EXPECT_EQ(log_future.Take(), "test log message");
}

}  // namespace one_time_tokens
