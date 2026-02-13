// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/one_time_token_service_impl.h"

#include "base/functional/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/sms_otp_backend.h"
#include "components/one_time_tokens/core/common/one_time_token_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::Optional;
using ::testing::Pair;
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

class MockGmailOtpBackend : public GmailOtpBackend {
 public:
  MOCK_METHOD(ExpiringSubscription,
              Subscribe,
              (base::Time expiration, Callback callback),
              (override));

  MOCK_METHOD(
      void,
      OnIncomingOneTimeTokenBackendTickle,
      (const GmailOtpBackend::EncryptedMessageReference& encrypted_message_reference),
      (override));

  // Simulates the reception of an OTP. This will run all pending callbacks from
  // `Subscribe`.
  void SimulateOtpArrived(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply) {
    for (auto& callback : callbacks_) {
      callback.Run(reply);
    }
  }

  // Notifies the mock backend that a subscription was created successfully.
  // This is needed for `SimulateOtpArrived` to have a callback to run.
  void AddMockSubscription(Callback callback) {
    callbacks_.push_back(std::move(callback));
  }

  bool HasPendingRetrieveGmailOtpCallbacks() { return !callbacks_.empty(); }

 private:
  // Store a copy of the callbacks passed to `Subscribe`.
  std::list<Callback> callbacks_;
};

// A helper class to collect results from the OneTimeTokenService callbacks.
class OneTimeTokenServiceTestObserver {
 public:
  explicit OneTimeTokenServiceTestObserver(
      std::optional<OneTimeTokenSource> expected_source = std::nullopt)
      : expected_source_(expected_source) {}

  void OnTokenReceived(
      OneTimeTokenSource backend,
      base::expected<OneTimeToken, OneTimeTokenRetrievalError> result) {
    if (!expected_source_.has_value() || expected_source_.value() == backend) {
      results_.emplace_back(backend, result);
    }
  }

  const std::vector<
      std::pair<OneTimeTokenSource,
                base::expected<OneTimeToken, OneTimeTokenRetrievalError>>>&
  results() const {
    return results_;
  }

 private:
  std::optional<OneTimeTokenSource> expected_source_;
  std::vector<
      std::pair<OneTimeTokenSource,
                base::expected<OneTimeToken, OneTimeTokenRetrievalError>>>
      results_;
};

}  // namespace

class OneTimeTokenServiceImplTest : public testing::Test {
 public:
  OneTimeTokenServiceImplTest()
      : gmail_otp_backend_(std::make_unique<MockGmailOtpBackend>()) {}
  ~OneTimeTokenServiceImplTest() override = default;

  void TearDown() override { feature_list_.Reset(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  MockSmsOtpBackend sms_otp_backend_;
  std::unique_ptr<MockGmailOtpBackend> gmail_otp_backend_;
};

// Test that with no backend, nothing happens.
TEST_F(OneTimeTokenServiceImplTest, NoBackend) {
  OneTimeTokenServiceImpl service(/*sms_otp_backend=*/nullptr,
                                  /*gmail_otp_backend=*/nullptr);
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
  OneTimeTokenServiceImpl service(&sms_otp_backend_, nullptr);
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
  OneTimeTokenServiceImpl service(&sms_otp_backend_, nullptr);
  OneTimeTokenServiceTestObserver observer;

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)));
  sms_otp_backend_.SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "123456", base::Time::Now()));

  ASSERT_THAT(observer.results(),
              ElementsAre(Pair(OneTimeTokenSource::kOnDeviceSms,
                               OneTimeTokenValueEq("123456"))));
}

// Test that after a successful fetch the backend is queried again.
TEST_F(OneTimeTokenServiceImplTest, BackendIsQueriedForFreshTokens) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_, nullptr);
  OneTimeTokenServiceTestObserver observer;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);
  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)));
  sms_otp_backend_.SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "1", base::Time::Now()));
  Mock::VerifyAndClearExpectations(&sms_otp_backend_);

  // After a few seconds, the backend should be queried a second time.
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);
  task_environment_.FastForwardBy(kSmsRefetchInterval);
  Mock::VerifyAndClearExpectations(&sms_otp_backend_);
  sms_otp_backend_.SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "2", base::Time::Now()));

  ASSERT_THAT(
      observer.results(),
      ElementsAre(
          Pair(OneTimeTokenSource::kOnDeviceSms, OneTimeTokenValueEq("1")),
          Pair(OneTimeTokenSource::kOnDeviceSms, OneTimeTokenValueEq("2"))));
}

// Test that multiple subscriptions only trigger one fetch.
TEST_F(OneTimeTokenServiceImplTest, MultipleSubscriptionsOneFetch) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_, nullptr);
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
  sms_otp_backend_.SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "123456", base::Time::Now()));

  ASSERT_EQ(observer1.results().size(), 1u);
  ASSERT_EQ(observer2.results().size(), 1u);
}

// Test that an expired subscription does not receive notifications.
TEST_F(OneTimeTokenServiceImplTest, ExpiredSubscription) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_, nullptr);
  OneTimeTokenServiceTestObserver observer;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)));

  task_environment_.FastForwardBy(base::Minutes(6));

  ASSERT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  sms_otp_backend_.SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "123456", base::Time::Now()));

  EXPECT_TRUE(observer.results().empty());
}

// Test that after all subscriptions expire, a new subscription triggers a new
// fetch.
TEST_F(OneTimeTokenServiceImplTest, NewSubscriptionAfterAllExpired) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_, nullptr);

  OneTimeTokenServiceTestObserver observer1;
  auto subscription1 = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer1)));
  sms_otp_backend_.SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "1", base::Time::Now()));
  ASSERT_THAT(observer1.results(),
              ElementsAre(Pair(OneTimeTokenSource::kOnDeviceSms,
                               OneTimeTokenValueEq("1"))));

  // Expire the subscription.
  task_environment_.FastForwardBy(base::Minutes(6));

  // Terminate the last callback to the backend.
  sms_otp_backend_.SimulateOtpArrived(
      base::unexpected(OneTimeTokenRetrievalError::kSmsOtpBackendError));
  EXPECT_FALSE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());

  OneTimeTokenServiceTestObserver observer2;
  auto subscription2 = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer2)));
  EXPECT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  sms_otp_backend_.SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "2", base::Time::Now()));
  ASSERT_THAT(observer2.results(),
              ElementsAre(Pair(OneTimeTokenSource::kOnDeviceSms,
                               OneTimeTokenValueEq("2"))));
}

// Test GetRecentOneTimeTokens returns cached tokens.
TEST_F(OneTimeTokenServiceImplTest, GetRecentOneTimeTokens) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_, nullptr);
  OneTimeTokenServiceTestObserver subscriber_observer;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&subscriber_observer)));

  ASSERT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  sms_otp_backend_.SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "123456", base::Time::Now()));

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
  OneTimeTokenServiceImpl service(&sms_otp_backend_, nullptr);
  OneTimeTokenServiceTestObserver subscriber_observer;

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&subscriber_observer)));

  ASSERT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  sms_otp_backend_.SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "123456", base::Time::Now()));

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
  OneTimeTokenServiceImpl service(&sms_otp_backend_, nullptr);
  OneTimeTokenServiceTestObserver observer;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)));

  ASSERT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  sms_otp_backend_.SimulateOtpArrived(
      base::unexpected(OneTimeTokenRetrievalError::kSmsOtpBackendError));

  ASSERT_EQ(observer.results().size(), 1u);
  EXPECT_EQ(observer.results()[0].first, OneTimeTokenSource::kOnDeviceSms);
  const auto& result = observer.results()[0].second;
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OneTimeTokenRetrievalError::kSmsOtpBackendError);
}

// Test GetCachedOneTimeTokens returns cached tokens, including expired ones.
TEST_F(OneTimeTokenServiceImplTest,
       GetCachedOneTimeTokens_ReturnsCachedTokens) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_, nullptr);
  OneTimeTokenServiceTestObserver subscriber_observer;

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&subscriber_observer)));

  sms_otp_backend_.SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "123456", base::Time::Now()));

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

// Test that subscribing triggers a fetch for Gmail.
TEST_F(OneTimeTokenServiceImplTest, GmailSubscriptionTriggersFetch) {
  OneTimeTokenServiceImpl service(/*sms_otp_backend=*/nullptr,
                                  gmail_otp_backend_.get());
  OneTimeTokenServiceTestObserver observer(OneTimeTokenSource::kGmail);

  EXPECT_CALL(*gmail_otp_backend_, Subscribe)
      .WillOnce([&](base::Time expiration, GmailOtpBackend::Callback callback) {
        gmail_otp_backend_->AddMockSubscription(std::move(callback));
        return ExpiringSubscription();
      });

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)));

  EXPECT_TRUE(gmail_otp_backend_->HasPendingRetrieveGmailOtpCallbacks());
}

// Test that a successful fetch notifies subscribers for Gmail.
TEST_F(OneTimeTokenServiceImplTest, GmailSuccessfulFetchNotifiesSubscribers) {
  OneTimeTokenServiceImpl service(/*sms_otp_backend=*/nullptr,
                                  gmail_otp_backend_.get());
  OneTimeTokenServiceTestObserver observer(OneTimeTokenSource::kGmail);

  EXPECT_CALL(*gmail_otp_backend_, Subscribe)
      .WillOnce([&](base::Time expiration, GmailOtpBackend::Callback callback) {
        gmail_otp_backend_->AddMockSubscription(std::move(callback));
        return ExpiringSubscription();
      });

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)));
  gmail_otp_backend_->SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kGmail, "654321", base::Time::Now()));

  EXPECT_THAT(observer.results(),
              ElementsAre(Pair(OneTimeTokenSource::kGmail,
                               OneTimeTokenValueEq("654321"))));
}

// Test that multiple Gmail subscriptions only trigger one fetch.
TEST_F(OneTimeTokenServiceImplTest, GmailMultipleSubscriptionsOneFetch) {
  OneTimeTokenServiceImpl service(/*sms_otp_backend=*/nullptr,
                                  gmail_otp_backend_.get());
  OneTimeTokenServiceTestObserver observer1(OneTimeTokenSource::kGmail);
  OneTimeTokenServiceTestObserver observer2(OneTimeTokenSource::kGmail);
  EXPECT_CALL(*gmail_otp_backend_, Subscribe)
      .WillOnce([&](base::Time expiration, GmailOtpBackend::Callback callback) {
        gmail_otp_backend_->AddMockSubscription(std::move(callback));
        return ExpiringSubscription();
      });

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

  EXPECT_TRUE(gmail_otp_backend_->HasPendingRetrieveGmailOtpCallbacks());
  gmail_otp_backend_->SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kGmail, "654321", base::Time::Now()));

  EXPECT_EQ(observer1.results().size(), 1u);
  EXPECT_EQ(observer2.results().size(), 1u);
}

// Test that an expired Gmail subscription does not receive notifications.
TEST_F(OneTimeTokenServiceImplTest, GmailExpiredSubscription) {
  OneTimeTokenServiceImpl service(/*sms_otp_backend=*/nullptr,
                                  gmail_otp_backend_.get());
  OneTimeTokenServiceTestObserver observer(OneTimeTokenSource::kGmail);

  EXPECT_CALL(*gmail_otp_backend_, Subscribe)
      .WillOnce([&](base::Time expiration, GmailOtpBackend::Callback callback) {
        gmail_otp_backend_->AddMockSubscription(std::move(callback));
        return ExpiringSubscription();
      });

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)));

  task_environment_.FastForwardBy(base::Minutes(6));

  EXPECT_TRUE(gmail_otp_backend_->HasPendingRetrieveGmailOtpCallbacks());
  gmail_otp_backend_->SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kGmail, "654321", base::Time::Now()));

  EXPECT_TRUE(observer.results().empty());
}

// Test that after all Gmail subscriptions expire, a new subscription triggers a
// new fetch.
TEST_F(OneTimeTokenServiceImplTest, GmailNewSubscriptionAfterAllExpired) {
  OneTimeTokenServiceImpl service(/*sms_otp_backend=*/nullptr,
                                  gmail_otp_backend_.get());

  EXPECT_CALL(*gmail_otp_backend_, Subscribe)
      .WillOnce([&](base::Time expiration, GmailOtpBackend::Callback callback) {
        gmail_otp_backend_->AddMockSubscription(std::move(callback));
        return ExpiringSubscription();
      });

  OneTimeTokenServiceTestObserver observer1(OneTimeTokenSource::kGmail);
  auto subscription1 = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer1)));
  gmail_otp_backend_->SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kGmail, "1", base::Time::Now()));
  EXPECT_THAT(observer1.results(), ElementsAre(Pair(OneTimeTokenSource::kGmail,
                                                    OneTimeTokenValueEq("1"))));

  // Expire the subscription.
  task_environment_.FastForwardBy(base::Minutes(6));

  // The first observer is expired and should not receive this notification.
  gmail_otp_backend_->SimulateOtpArrived(base::unexpected(
      OneTimeTokenRetrievalError::kSmsOtpBackendInitializationFailed));
  EXPECT_EQ(observer1.results().size(), 1u);

  // A new subscription should not trigger a new call to the backend, because
  // the backend subscription is still active.
  OneTimeTokenServiceTestObserver observer2(OneTimeTokenSource::kGmail);
  auto subscription2 = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer2)));
  gmail_otp_backend_->SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kGmail, "2", base::Time::Now()));

  // The new observer should have received the token.
  EXPECT_THAT(observer2.results(), ElementsAre(Pair(OneTimeTokenSource::kGmail,
                                                    OneTimeTokenValueEq("2"))));
  // The old observer should not have received the new token.
  EXPECT_EQ(observer1.results().size(), 1u);
}

// Test GetRecentOneTimeTokens returns cached Gmail tokens.
TEST_F(OneTimeTokenServiceImplTest, GmailGetRecentOneTimeTokens) {
  OneTimeTokenServiceImpl service(/*sms_otp_backend=*/nullptr,
                                  gmail_otp_backend_.get());
  OneTimeTokenServiceTestObserver subscriber_observer(
      OneTimeTokenSource::kGmail);

  EXPECT_CALL(*gmail_otp_backend_, Subscribe)
      .WillOnce([&](base::Time expiration, GmailOtpBackend::Callback callback) {
        gmail_otp_backend_->AddMockSubscription(std::move(callback));
        return ExpiringSubscription();
      });

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&subscriber_observer)));

  EXPECT_TRUE(gmail_otp_backend_->HasPendingRetrieveGmailOtpCallbacks());
  gmail_otp_backend_->SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kGmail, "654321", base::Time::Now()));

  EXPECT_THAT(subscriber_observer.results(),
              ElementsAre(Pair(OneTimeTokenSource::kGmail,
                               OneTimeTokenValueEq("654321"))));

  // Now, get recent tokens.
  OneTimeTokenServiceTestObserver recent_observer(OneTimeTokenSource::kGmail);
  service.GetRecentOneTimeTokens(
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&recent_observer)));
  EXPECT_THAT(recent_observer.results(),
              ElementsAre(Pair(OneTimeTokenSource::kGmail,
                               OneTimeTokenValueEq("654321"))));
}

// Test GetRecentOneTimeTokens does not return expired Gmail tokens.
TEST_F(OneTimeTokenServiceImplTest, GmailGetRecentOneTimeTokens_Expired) {
  OneTimeTokenServiceImpl service(/*sms_otp_backend=*/nullptr,
                                  gmail_otp_backend_.get());
  OneTimeTokenServiceTestObserver subscriber_observer(
      OneTimeTokenSource::kGmail);

  EXPECT_CALL(*gmail_otp_backend_, Subscribe)
      .WillOnce([&](base::Time expiration, GmailOtpBackend::Callback callback) {
        gmail_otp_backend_->AddMockSubscription(std::move(callback));
        return ExpiringSubscription();
      });

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&subscriber_observer)));

  EXPECT_TRUE(gmail_otp_backend_->HasPendingRetrieveGmailOtpCallbacks());
  gmail_otp_backend_->SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kGmail, "654321", base::Time::Now()));

  EXPECT_EQ(subscriber_observer.results().size(), 1u);

  // Expire the token.
  task_environment_.FastForwardBy(base::Minutes(6));

  // Now, get recent tokens.
  OneTimeTokenServiceTestObserver recent_observer(OneTimeTokenSource::kGmail);
  service.GetRecentOneTimeTokens(
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&recent_observer)));

  EXPECT_TRUE(recent_observer.results().empty());
}

// Test that an error from the Gmail backend is propagated to subscribers.
TEST_F(OneTimeTokenServiceImplTest, GmailNewFetchAfterCompletion) {
  OneTimeTokenServiceImpl service(/*sms_otp_backend=*/nullptr,
                                  gmail_otp_backend_.get());
  OneTimeTokenServiceTestObserver observer(OneTimeTokenSource::kGmail);

  EXPECT_CALL(*gmail_otp_backend_, Subscribe)
      .WillOnce([&](base::Time expiration, GmailOtpBackend::Callback callback) {
        gmail_otp_backend_->AddMockSubscription(std::move(callback));
        return ExpiringSubscription();
      });

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer)));

  EXPECT_TRUE(gmail_otp_backend_->HasPendingRetrieveGmailOtpCallbacks());
  gmail_otp_backend_->SimulateOtpArrived(base::unexpected(
      OneTimeTokenRetrievalError::kSmsOtpBackendInitializationFailed));

  EXPECT_EQ(observer.results().size(), 1u);
  EXPECT_EQ(observer.results()[0].first, OneTimeTokenSource::kGmail);
  const auto& result = observer.results()[0].second;
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            OneTimeTokenRetrievalError::kSmsOtpBackendInitializationFailed);
}

// Test GetCachedOneTimeTokens returns cached Gmail tokens, including expired
// ones.
TEST_F(OneTimeTokenServiceImplTest,
       GmailGetCachedOneTimeTokens_ReturnsCachedTokens) {
  OneTimeTokenServiceImpl service(/*sms_otp_backend=*/nullptr,
                                  gmail_otp_backend_.get());
  OneTimeTokenServiceTestObserver subscriber_observer(
      OneTimeTokenSource::kGmail);

  EXPECT_CALL(*gmail_otp_backend_, Subscribe)
      .WillOnce([&](base::Time expiration, GmailOtpBackend::Callback callback) {
        gmail_otp_backend_->AddMockSubscription(std::move(callback));
        return ExpiringSubscription();
      });

  auto subscription = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&subscriber_observer)));

  gmail_otp_backend_->SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kGmail, "654321", base::Time::Now()));

  // Verify that the token is returned by GetCachedOneTimeTokens.
  std::vector<OneTimeToken> cached_tokens = service.GetCachedOneTimeTokens();
  EXPECT_EQ(cached_tokens.size(), 1u);
  EXPECT_EQ(cached_tokens[0].value(), "654321");

  // Expire the token.
  task_environment_.FastForwardBy(base::Minutes(6));

  // Verify that the expired token is still returned by GetCachedOneTimeTokens.
  cached_tokens = service.GetCachedOneTimeTokens();
  EXPECT_EQ(cached_tokens.size(), 1u);
  EXPECT_EQ(cached_tokens[0].value(), "654321");
}

// Test that both Gmail and SMS subscriptions trigger separate fetches.
TEST_F(OneTimeTokenServiceImplTest,
       GmailAndSmsSubscriptionsTriggerSeparateFetches) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_, gmail_otp_backend_.get());
  OneTimeTokenServiceTestObserver observer_sms(
      OneTimeTokenSource::kOnDeviceSms);
  OneTimeTokenServiceTestObserver observer_gmail(OneTimeTokenSource::kGmail);

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);
  EXPECT_CALL(*gmail_otp_backend_, Subscribe)
      .WillOnce([&](base::Time expiration, GmailOtpBackend::Callback callback) {
        gmail_otp_backend_->AddMockSubscription(std::move(callback));
        return ExpiringSubscription();
      });

  auto subscription_sms = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer_sms)));
  auto subscription_gmail = service.Subscribe(
      base::Time::Now() + base::Minutes(5),
      base::BindRepeating(&OneTimeTokenServiceTestObserver::OnTokenReceived,
                          base::Unretained(&observer_gmail)));

  EXPECT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  EXPECT_TRUE(gmail_otp_backend_->HasPendingRetrieveGmailOtpCallbacks());

  sms_otp_backend_.SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "123456", base::Time::Now()));
  gmail_otp_backend_->SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kGmail, "654321", base::Time::Now()));

  EXPECT_THAT(observer_sms.results(),
              ElementsAre(Pair(OneTimeTokenSource::kOnDeviceSms,
                               OneTimeTokenValueEq("123456"))));
  EXPECT_THAT(observer_gmail.results(),
              ElementsAre(Pair(OneTimeTokenSource::kGmail,
                               OneTimeTokenValueEq("654321"))));
}

// Test that RequestOneTimeToken returns a token when the backend succeeds.
TEST_F(OneTimeTokenServiceImplTest, RequestOneTimeToken_Success) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_, nullptr);
  base::MockOnceCallback<void(std::optional<OneTimeToken>)> callback;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);
  service.RequestOneTimeToken(base::Seconds(1), callback.Get());

  EXPECT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  EXPECT_CALL(callback,
              Run(Optional(Property(&OneTimeToken::value, Eq("123456")))));
  sms_otp_backend_.SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "123456", base::Time::Now()));
}

// Test that RequestOneTimeToken returns nullopt when the backend fails.
TEST_F(OneTimeTokenServiceImplTest, RequestOneTimeToken_Failure) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_, nullptr);
  base::MockOnceCallback<void(std::optional<OneTimeToken>)> callback;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);
  service.RequestOneTimeToken(base::Seconds(1), callback.Get());

  EXPECT_CALL(callback, Run(Eq(std::nullopt)));
  sms_otp_backend_.SimulateOtpArrived(
      base::unexpected(OneTimeTokenRetrievalError::kSmsOtpBackendError));
}

// Test that RequestOneTimeToken returns nullopt when there is no backend.
TEST_F(OneTimeTokenServiceImplTest, RequestOneTimeToken_NoBackend) {
  OneTimeTokenServiceImpl service(nullptr, nullptr);
  base::MockOnceCallback<void(std::optional<OneTimeToken>)> callback;

  EXPECT_CALL(callback, Run(Eq(std::nullopt)));
  service.RequestOneTimeToken(base::Seconds(1), callback.Get());
}

// Test that RequestOneTimeToken returns nullopt when the timeout is reached.
TEST_F(OneTimeTokenServiceImplTest, RequestOneTimeToken_Timeout) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_, nullptr);
  base::MockOnceCallback<void(std::optional<OneTimeToken>)> callback;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);
  service.RequestOneTimeToken(base::Milliseconds(100), callback.Get());

  EXPECT_TRUE(sms_otp_backend_.HasPendingRetrieveSmsOtpCallbacks());
  EXPECT_CALL(callback, Run(Eq(std::nullopt)));
  task_environment_.FastForwardBy(base::Milliseconds(101));
}

// Test that if the backend responds after the timeout, it doesn't crash and the
// second response is ignored.
TEST_F(OneTimeTokenServiceImplTest, RequestOneTimeToken_LateResponse) {
  OneTimeTokenServiceImpl service(&sms_otp_backend_, nullptr);
  base::MockOnceCallback<void(std::optional<OneTimeToken>)> callback;

  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtpCalled).Times(1);
  service.RequestOneTimeToken(base::Milliseconds(100), callback.Get());

  // Fast forward past the timeout.
  EXPECT_CALL(callback, Run(Eq(std::nullopt)));
  task_environment_.FastForwardBy(base::Milliseconds(101));

  // Now simulate a late response from the backend.
  // This should not call the callback again and should not crash.
  sms_otp_backend_.SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "123456", base::Time::Now()));
}

}  // namespace one_time_tokens
