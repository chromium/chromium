// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/one_time_token_service_impl.h"

#include <variant>

#include "base/functional/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/sms_otp_backend.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

namespace {

using base::test::RunOnceCallback;
using testing::_;
using testing::ElementsAre;
using testing::Mock;
using testing::Pair;
using testing::SaveArg;

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
      base::OnceCallback<void(const OtpFetchReply&)> callback) override {
    callbacks_.push_back(std::move(callback));
    RetrieveSmsOtpCalled();
  }

  bool HasPendingRetrieveSmsOtpCallbacks() { return !callbacks_.empty(); }

  void SimulateOtpArrived(const OtpFetchReply& reply) {
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
  std::list<base::OnceCallback<void(const OtpFetchReply&)>> callbacks_;
};

// A helper class to collect results from the OneTimeTokenService callbacks.
class OneTimeTokenServiceTestObserver {
 public:
  void OnTokenReceived(
      OneTimeTokenSource backend,
      base::expected<OneTimeToken, OneTimeTokenRetrievalError> result) {
    results_.emplace_back(backend, result);
  }

  const std::vector<
      std::pair<OneTimeTokenSource,
                base::expected<OneTimeToken, OneTimeTokenRetrievalError>>>&
  results() const {
    return results_;
  }

 private:
  std::vector<
      std::pair<OneTimeTokenSource,
                base::expected<OneTimeToken, OneTimeTokenRetrievalError>>>
      results_;
};

}  // namespace

class OneTimeTokenServiceImplTest : public testing::Test {
 public:
  OneTimeTokenServiceImplTest() = default;
  ~OneTimeTokenServiceImplTest() override = default;

 protected:
  OtpFetchReply GetOtpReply(const std::string& otp_value) {
    return OtpFetchReply(
        OneTimeToken(OneTimeTokenType::kSmsOtp, otp_value, base::Time::Now()),
        /*request_complete=*/true);
  }

  OtpFetchReply GetDefaultOtpFetchReply() { return GetOtpReply("123456"); }

  OtpFetchReply GetOtpFailureReply() {
    return OtpFetchReply(std::nullopt,
                         /*request_complete=*/false);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockSmsOtpBackend sms_otp_backend_;
};

// Test that with no backend, nothing happens.
TEST_F(OneTimeTokenServiceImplTest, NoBackend) {
  OneTimeTokenServiceImpl service(/*sms_otp_backend=*/nullptr);
  OneTimeTokenServiceTestObserver observer;

  // GetRecentOneTimeTokens should not call the callback.
  service.GetRecentOneTimeTokens(
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)));
  EXPECT_TRUE(observer.results().empty());

  // Subscribe should not trigger any backend calls.
  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)));
  EXPECT_TRUE(observer.results().empty());
}

// Test that subscribing triggers a fetch.
TEST_F(OneTimeTokenServiceImplTest, SubscriptionTriggersFetch) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_);
  OneTimeTokenServiceTestObserver observer;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)));

  ASSERT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
}

// Test that a successful fetch notifies subscribers.
TEST_F(OneTimeTokenServiceImplTest, SuccessfulFetchNotifiesSubscribers) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_);
  OneTimeTokenServiceTestObserver observer;

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)));
  sms_otp_backend_.SimulateOtpArrived(GetDefaultOtpFetchReply());

  ASSERT_THAT(observer.results(),
              ElementsAre(Pair(OneTimeTokenSource::kOnDeviceSms,
                               OneTimeTokenValueEq("123456"))));
}

// Test that after a successful fetch the backend is queried again.
TEST_F(OneTimeTokenServiceImplTest, BackendIsQueriedForFreshTokens) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_);
  OneTimeTokenServiceTestObserver observer;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);
  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)));
  sms_otp_backend_.SimulateOtpArrived(GetOtpReply("1"));
  Mock::VerifyAndClearExpectations(&sms_otp_backend_);

  // After a few seconds, the backend should be queried a second time.
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);
  task_environment_.FastForwardBy(kSmsRefetchInterval);
  Mock::VerifyAndClearExpectations(&sms_otp_backend_);
  sms_otp_backend_.SimulateOtpArrived(GetOtpReply("2"));

  ASSERT_THAT(
      observer.results(),
      ElementsAre(
          Pair(OneTimeTokenSource::kOnDeviceSms, OneTimeTokenValueEq("1")),
          Pair(OneTimeTokenSource::kOnDeviceSms, OneTimeTokenValueEq("2"))));
}

// Test that multiple subscriptions only trigger one fetch.
TEST_F(OneTimeTokenServiceImplTest, MultipleSubscriptionsOneFetch) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_);
  OneTimeTokenServiceTestObserver observer1;
  OneTimeTokenServiceTestObserver observer2;
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);

  auto subscription1 = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer1)));
  // While the first request is in progress, a second subscription should not
  // trigger a new request.
  auto subscription2 = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer2)));

  ASSERT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  sms_otp_backend_.SimulateOtpArrived(GetDefaultOtpFetchReply());

  ASSERT_EQ(observer1.results().size(), 1u);
  ASSERT_EQ(observer2.results().size(), 1u);
}

// Test that an expired subscription does not receive notifications.
TEST_F(OneTimeTokenServiceImplTest, ExpiredSubscription) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_);
  OneTimeTokenServiceTestObserver observer;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)));

  task_environment_.FastForwardBy(base::Minutes(6));

  ASSERT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  sms_otp_backend_.SimulateOtpArrived(GetDefaultOtpFetchReply());

  EXPECT_TRUE(observer.results().empty());
}

// Test that after all subscriptions expire, a new subscription triggers a new
// fetch.
TEST_F(OneTimeTokenServiceImplTest, NewSubscriptionAfterAllExpired) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_);

  OneTimeTokenServiceTestObserver observer1;
  auto subscription1 = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer1)));
  sms_otp_backend_.SimulateOtpArrived(GetOtpReply("1"));
  ASSERT_THAT(observer1.results(),
              ElementsAre(Pair(OneTimeTokenSource::kOnDeviceSms,
                               OneTimeTokenValueEq("1"))));

  // Expire the subscription.
  task_environment_.FastForwardBy(base::Minutes(6));

  // Terminate the last callback to the backend.
  sms_otp_backend_.SimulateOtpArrived(GetOtpFailureReply());
  EXPECT_FALSE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());

  OneTimeTokenServiceTestObserver observer2;
  auto subscription2 = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer2)));
  EXPECT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  sms_otp_backend_.SimulateOtpArrived(GetOtpReply("2"));
  ASSERT_THAT(observer2.results(),
              ElementsAre(Pair(OneTimeTokenSource::kOnDeviceSms,
                               OneTimeTokenValueEq("2"))));
}

// Test GetRecentOneTimeTokens returns cached tokens.
TEST_F(OneTimeTokenServiceImplTest, GetRecentOneTimeTokens) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_);
  OneTimeTokenServiceTestObserver subscriber_observer;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&subscriber_observer)));

  ASSERT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  sms_otp_backend_.SimulateOtpArrived(GetDefaultOtpFetchReply());

  ASSERT_THAT(subscriber_observer.results(),
              ElementsAre(Pair(OneTimeTokenSource::kOnDeviceSms,
                               OneTimeTokenValueEq("123456"))));

  // Now, get recent tokens.
  OneTimeTokenServiceTestObserver recent_observer;
  service.GetRecentOneTimeTokens(
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&recent_observer)));
  EXPECT_THAT(recent_observer.results(),
              ElementsAre(Pair(OneTimeTokenSource::kOnDeviceSms,
                               OneTimeTokenValueEq("123456"))));
}

// Test GetRecentOneTimeTokens does not return expired tokens.
TEST_F(OneTimeTokenServiceImplTest, GetRecentOneTimeTokens_Expired) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_);
  OneTimeTokenServiceTestObserver subscriber_observer;

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&subscriber_observer)));

  ASSERT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  sms_otp_backend_.SimulateOtpArrived(GetDefaultOtpFetchReply());

  ASSERT_EQ(subscriber_observer.results().size(), 1u);

  // Expire the token.
  task_environment_.FastForwardBy(base::Minutes(6));

  // Now, get recent tokens.
  OneTimeTokenServiceTestObserver recent_observer;
  service.GetRecentOneTimeTokens(
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&recent_observer)));

  EXPECT_TRUE(recent_observer.results().empty());
}

// Test that an error from the backend is propagated to subscribers.
TEST_F(OneTimeTokenServiceImplTest, NewFetchAfterCompletion) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_);
  OneTimeTokenServiceTestObserver observer;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)));

  ASSERT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  sms_otp_backend_.SimulateOtpArrived(GetOtpFailureReply());

  ASSERT_EQ(observer.results().size(), 1u);
  EXPECT_EQ(observer.results()[0].first, OneTimeTokenSource::kOnDeviceSms);
  const auto& result = observer.results()[0].second;
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OneTimeTokenRetrievalError::kUnknown);
}

// Test GetCachedOneTimeTokens returns cached tokens, including expired ones.
TEST_F(OneTimeTokenServiceImplTest,
       GetCachedOneTimeTokens_ReturnsCachedTokens) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_);
  OneTimeTokenServiceTestObserver subscriber_observer;

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&subscriber_observer)));

  sms_otp_backend_.SimulateOtpArrived(GetOtpReply("123456"));

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

}  // namespace one_time_tokens
