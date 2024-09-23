// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/invalidation_listener_impl.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/test_support/fake_registration_token_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::WithArg;

namespace invalidation {

namespace {

constexpr char kFakeSenderId[] = "fake_sender_id";
constexpr char kTestLogPrefix[] = "test";
constexpr char kFakeRegistrationToken[] = "fake_registration_token";
constexpr char kMessagePayload[] = "payload";
constexpr base::TimeDelta kMessageIssueTimeDeltaSinceEpoch =
    base::Milliseconds(123456789);
constexpr base::Time kMessageIssueTimeDelta =
    base::Time::UnixEpoch() + kMessageIssueTimeDeltaSinceEpoch;

class MockInstanceIDDriver : public instance_id::InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}
  ~MockInstanceIDDriver() override = default;

  MOCK_METHOD(instance_id::InstanceID*,
              GetInstanceID,
              (const std::string& app_id),
              (override));
  MOCK_METHOD(void, RemoveInstanceID, (const std::string& app_id), (override));
  MOCK_METHOD(bool,
              ExistsInstanceID,
              (const std::string& app_id),
              (const override));
};

class MockInstanceID : public instance_id::InstanceID {
 public:
  MockInstanceID() : InstanceID("app_id", /*gcm_driver=*/nullptr) {}
  ~MockInstanceID() override = default;
  MOCK_METHOD(void, GetID, (GetIDCallback callback), (override));
  MOCK_METHOD(void,
              GetCreationTime,
              (GetCreationTimeCallback callback),
              (override));
  MOCK_METHOD(void,
              GetToken,
              (const std::string& authorized_entity,
               const std::string& scope,
               base::TimeDelta time_to_live,
               std::set<Flags> flags,
               GetTokenCallback callback),
              (override));
  MOCK_METHOD(void,
              ValidateToken,
              (const std::string& authorized_entity,
               const std::string& scope,
               const std::string& token,
               ValidateTokenCallback callback),
              (override));

 protected:
  MOCK_METHOD(void,
              DeleteTokenImpl,
              (const std::string& authorized_entity,
               const std::string& scope,
               DeleteTokenCallback callback),
              (override));
  MOCK_METHOD(void, DeleteIDImpl, (DeleteIDCallback callback), (override));
};

class FakeObserver : public InvalidationListener::Observer {
 public:
  FakeObserver() = default;

  void OnExpectationChanged(InvalidationsExpected expected) override {
    current_expectation_ = expected;
  }
  void OnInvalidationReceived(const DirectInvalidation& invalidation) override {
    received_invalidations_.push_back(invalidation);
  }

  std::string GetType() const override { return "fake_type"; }

  InvalidationsExpected get_current_expectation() const {
    return current_expectation_;
  }
  void set_current_expectation(InvalidationsExpected expectation) {
    current_expectation_ = expectation;
  }

  auto CountSpecificInvalidation(const std::string& payload,
                                 int64_t version,
                                 base::Time issue_timestamp) {
    return std::count_if(
        received_invalidations_.begin(), received_invalidations_.end(),
        [&payload, &version, &issue_timestamp,
         this](const DirectInvalidation& invalidation) {
          return invalidation.type() == GetType() &&
                 invalidation.payload() == payload &&
                 invalidation.version() == version &&
                 invalidation.issue_timestamp() == issue_timestamp;
        });
  }

 private:
  InvalidationsExpected current_expectation_;
  std::vector<DirectInvalidation> received_invalidations_;
};

}  // namespace

class InvalidationListenerImplTest : public testing::Test {
 public:
  InvalidationListenerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ON_CALL(mock_instance_id__driver_,
            GetInstanceID(InvalidationListener::kFmAppId))
        .WillByDefault(Return(&mock_instance_id_));
  }
  void TearDown() override {}

 protected:
  void SetRegistrationTokenFetchState(const std::string& registration_token,
                                      instance_id::InstanceID::Result result) {
    ON_CALL(mock_instance_id_,
            GetToken(/*authorized_entity=*/kFakeSenderId,
                     /*scope=*/instance_id::kGCMScope,
                     /*time_to_live=*/
                     InvalidationListenerImpl::kRegistrationTokenTimeToLive,
                     /*flags=*/_, /*callback=*/_))
        .WillByDefault(
            // Call the callback with `registration_token` and `result` as
            // arguments.
            WithArg<4>([registration_token,
                        result](MockInstanceID::GetTokenCallback callback) {
              std::move(callback).Run(registration_token, result);
            }));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  FakeRegistrationTokenHandler fake_token_handler_;
  gcm::FakeGCMDriver fake_gcm_driver_;
  testing::NiceMock<MockInstanceIDDriver> mock_instance_id__driver_;
  testing::NiceMock<MockInstanceID> mock_instance_id_;
};

TEST_F(InvalidationListenerImplTest,
       SuccessfullyStartsAndNotifiesTokenHandler) {
  SetRegistrationTokenFetchState(kFakeRegistrationToken,
                                 instance_id::InstanceID::SUCCESS);
  InvalidationListenerImpl listener(&fake_gcm_driver_,
                                    &mock_instance_id__driver_, kFakeSenderId,
                                    kTestLogPrefix);

  listener.Start(&fake_token_handler_);

  EXPECT_EQ(fake_token_handler_.get_registration_token(),
            kFakeRegistrationToken);
}

TEST_F(InvalidationListenerImplTest,
       SuccessfulUploadStatusEmitsKYesExpectation) {
  SetRegistrationTokenFetchState(kFakeRegistrationToken,
                                 instance_id::InstanceID::SUCCESS);
  InvalidationListenerImpl listener(&fake_gcm_driver_,
                                    &mock_instance_id__driver_, kFakeSenderId,
                                    kTestLogPrefix);
  FakeObserver observer;
  listener.AddObserver(&observer);
  // Explicitly setting current expectation to verify that the listener
  // actually emits `::kYes` value.
  observer.set_current_expectation(InvalidationsExpected::kMaybe);

  listener.Start(&fake_token_handler_);
  listener.SetRegistrationUploadStatus(
      InvalidationListener::RegistrationTokenUploadStatus::kSucceeded);

  EXPECT_EQ(observer.get_current_expectation(), InvalidationsExpected::kYes);
  listener.RemoveObserver(&observer);
}

TEST_F(InvalidationListenerImplTest, FailedUploadStatusEmitsKMaybeExpectation) {
  SetRegistrationTokenFetchState(kFakeRegistrationToken,
                                 instance_id::InstanceID::SUCCESS);
  InvalidationListenerImpl listener(&fake_gcm_driver_,
                                    &mock_instance_id__driver_, kFakeSenderId,
                                    kTestLogPrefix);
  FakeObserver observer;
  listener.AddObserver(&observer);
  // Explicitly setting current expectation to verify that listener actually
  // emits `::kMaybe` value.
  observer.set_current_expectation(InvalidationsExpected::kYes);

  listener.Start(&fake_token_handler_);
  listener.SetRegistrationUploadStatus(
      InvalidationListener::RegistrationTokenUploadStatus::kFailed);

  EXPECT_EQ(observer.get_current_expectation(), InvalidationsExpected::kMaybe);
  listener.RemoveObserver(&observer);
}

TEST_F(InvalidationListenerImplTest,
       SuccessfulTokenUpdateEmitsKYesExpectation) {
  SetRegistrationTokenFetchState(kFakeRegistrationToken,
                                 instance_id::InstanceID::SUCCESS);
  InvalidationListenerImpl listener(&fake_gcm_driver_,
                                    &mock_instance_id__driver_, kFakeSenderId,
                                    kTestLogPrefix);
  FakeObserver observer;
  listener.AddObserver(&observer);

  listener.Start(&fake_token_handler_);
  listener.SetRegistrationUploadStatus(
      InvalidationListener::RegistrationTokenUploadStatus::kSucceeded);
  EXPECT_EQ(observer.get_current_expectation(), InvalidationsExpected::kYes);
  // Explicitly setting current expectation to verify that listener actually
  // emits `::kYes` value.
  observer.set_current_expectation(InvalidationsExpected::kMaybe);
  // Simulate token refetch.
  std::string new_fake_registration_token = "new_fake_registration_token";
  SetRegistrationTokenFetchState(new_fake_registration_token,
                                 instance_id::InstanceID::SUCCESS);
  task_environment_.FastForwardBy(
      InvalidationListenerImpl::kRegistrationTokenValidationPeriod);

  EXPECT_EQ(observer.get_current_expectation(), InvalidationsExpected::kYes);
  listener.RemoveObserver(&observer);
}

TEST_F(InvalidationListenerImplTest,
       JustSubscribedObserverReceivesExpectationStatus) {
  SetRegistrationTokenFetchState(kFakeRegistrationToken,
                                 instance_id::InstanceID::SUCCESS);
  InvalidationListenerImpl listener(&fake_gcm_driver_,
                                    &mock_instance_id__driver_, kFakeSenderId,
                                    kTestLogPrefix);
  listener.Start(&fake_token_handler_);
  listener.SetRegistrationUploadStatus(
      InvalidationListener::RegistrationTokenUploadStatus::kSucceeded);
  FakeObserver observer;
  observer.set_current_expectation(InvalidationsExpected::kMaybe);

  listener.AddObserver(&observer);

  EXPECT_EQ(observer.get_current_expectation(), InvalidationsExpected::kYes);
  listener.RemoveObserver(&observer);
}

TEST_F(InvalidationListenerImplTest,
       JustSubscribedObserversCorrectlyReceiveCachedInvalidations) {
  InvalidationListenerImpl listener(&fake_gcm_driver_,
                                    &mock_instance_id__driver_, kFakeSenderId,
                                    kTestLogPrefix);
  FakeObserver observer;
  gcm::IncomingMessage message_for_fake_observer;
  message_for_fake_observer.data["type"] = observer.GetType();
  message_for_fake_observer.data["payload"] = kMessagePayload;
  message_for_fake_observer.data["issue_timestamp_ms"] =
      base::NumberToString(kMessageIssueTimeDeltaSinceEpoch.InMilliseconds());
  listener.OnMessage(InvalidationListener::kFmAppId, message_for_fake_observer);
  // Setting up another message not intended for `observer`, to check that
  // `InvalidationListener` correctly redirects cached messages.
  gcm::IncomingMessage message_not_for_fake_observer;
  message_not_for_fake_observer.data["type"] = "another_type";
  message_not_for_fake_observer.data["payload"] = kMessagePayload;
  message_not_for_fake_observer.data["issue_timestamp_ms"] =
      base::NumberToString(kMessageIssueTimeDeltaSinceEpoch.InMilliseconds());
  listener.OnMessage(InvalidationListener::kFmAppId,
                     message_not_for_fake_observer);

  listener.AddObserver(&observer);

  EXPECT_EQ(
      observer.CountSpecificInvalidation(
          kMessagePayload, kMessageIssueTimeDeltaSinceEpoch.InMicroseconds(),
          kMessageIssueTimeDelta),
      1);
  listener.RemoveObserver(&observer);
}

TEST_F(InvalidationListenerImplTest,
       SubscribedObserversCorrectlyReceiveMessages) {
  InvalidationListenerImpl listener(&fake_gcm_driver_,
                                    &mock_instance_id__driver_, kFakeSenderId,
                                    kTestLogPrefix);
  FakeObserver observer;
  listener.AddObserver(&observer);

  gcm::IncomingMessage message_for_fake_observer;
  message_for_fake_observer.data["type"] = observer.GetType();
  message_for_fake_observer.data["payload"] = kMessagePayload;
  message_for_fake_observer.data["issue_timestamp_ms"] =
      base::NumberToString(kMessageIssueTimeDeltaSinceEpoch.InMilliseconds());
  listener.OnMessage(InvalidationListener::kFmAppId, message_for_fake_observer);
  // Setting up another message not intended for `observer`, to check that
  // `InvalidationListener` correctly redirects incoming messages.
  gcm::IncomingMessage message_for_another_observer;
  message_for_another_observer.data["type"] = "another_type";
  message_for_another_observer.data["payload"] = kMessagePayload;
  message_for_another_observer.data["issue_timestamp_ms"] =
      base::NumberToString(kMessageIssueTimeDeltaSinceEpoch.InMilliseconds());
  listener.OnMessage(InvalidationListener::kFmAppId,
                     message_for_another_observer);

  EXPECT_EQ(
      observer.CountSpecificInvalidation(
          kMessagePayload, kMessageIssueTimeDeltaSinceEpoch.InMicroseconds(),
          kMessageIssueTimeDelta),
      1);
  listener.RemoveObserver(&observer);
}

TEST_F(InvalidationListenerImplTest, ListenerProperlyCleansUpCachedMessages) {
  InvalidationListenerImpl listener(&fake_gcm_driver_,
                                    &mock_instance_id__driver_, kFakeSenderId,
                                    kTestLogPrefix);
  FakeObserver observer;
  gcm::IncomingMessage message_for_fake_observer;
  message_for_fake_observer.data["type"] = observer.GetType();
  message_for_fake_observer.data["payload"] = kMessagePayload;
  message_for_fake_observer.data["issue_timestamp_ms"] =
      base::NumberToString(kMessageIssueTimeDeltaSinceEpoch.InMilliseconds());
  listener.OnMessage(InvalidationListener::kFmAppId, message_for_fake_observer);
  listener.AddObserver(&observer);
  EXPECT_EQ(
      observer.CountSpecificInvalidation(
          kMessagePayload, kMessageIssueTimeDeltaSinceEpoch.InMicroseconds(),
          kMessageIssueTimeDelta),
      1);

  // Resubscribe.
  listener.RemoveObserver(&observer);
  listener.AddObserver(&observer);

  EXPECT_EQ(
      observer.CountSpecificInvalidation(
          kMessagePayload, kMessageIssueTimeDeltaSinceEpoch.InMicroseconds(),
          kMessageIssueTimeDelta),
      1);
  listener.RemoveObserver(&observer);
}

TEST_F(InvalidationListenerImplTest, ShutsdownCorrectly) {
  InvalidationListenerImpl listener(&fake_gcm_driver_,
                                    &mock_instance_id__driver_, kFakeSenderId,
                                    kTestLogPrefix);

  listener.Shutdown();

  EXPECT_EQ(fake_gcm_driver_.GetAppHandler(kFakeSenderId), nullptr);
}

}  // namespace invalidation
