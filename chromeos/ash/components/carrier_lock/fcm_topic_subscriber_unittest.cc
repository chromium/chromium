// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/carrier_lock/fcm_topic_subscriber_impl.h"

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/gcm_driver/instance_id/fake_gcm_driver_for_instance_id.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::carrier_lock {

namespace {

const char kEmbeddedAppIdKey[] = "gcmb";
const char kFcmAppId[] = "com.google.chromeos.carrier_lock";
const char kFcmSenderId[] = "1067228791894";
const char kFcmTopic[] = "/topics/testtopic";

}  // namespace

class FcmTopicSubscriberTest : public testing::Test {
 public:
  FcmTopicSubscriberTest() = default;
  FcmTopicSubscriberTest(const FcmTopicSubscriberTest&) = delete;
  FcmTopicSubscriberTest& operator=(const FcmTopicSubscriberTest&) = delete;
  ~FcmTopicSubscriberTest() override = default;

  void NotificationCallback(bool) {}

 protected:
  // testing::Test:
  void SetUp() override {
    fcm_ = std::make_unique<FcmTopicSubscriberImpl>(&gcm_driver_, kFcmAppId,
                                                    kFcmSenderId, nullptr);
  }

  void TearDown() override { fcm_.reset(); }

  std::unique_ptr<FcmTopicSubscriber> fcm_;
  base::test::TaskEnvironment task_environment_;
  instance_id::FakeGCMDriverForInstanceID gcm_driver_;
};

TEST_F(FcmTopicSubscriberTest, CarrierLockSubscribeTopicSuccess) {
  base::test::TestFuture<Result> future;

  // Request token and subscribe with valid topic
  fcm_->SubscribeTopic(
      kFcmTopic,
      base::BindRepeating(&FcmTopicSubscriberTest::NotificationCallback,
                          base::Unretained(this)),
      future.GetCallback());

  // Wait for callback
  EXPECT_EQ(Result::kSuccess, future.Get());
  EXPECT_NE(std::string(), fcm_->token());
}

TEST_F(FcmTopicSubscriberTest, CarrierLockTestNotifications) {
  base::test::TestFuture<Result> future;
  base::test::RepeatingTestFuture<bool> notifications;

  // Request token and subscribe with valid topic
  fcm_->SubscribeTopic(kFcmTopic, notifications.GetCallback(),
                       future.GetCallback());

  // Wait for subscription callback
  EXPECT_EQ(Result::kSuccess, future.Take());
  EXPECT_NE(std::string(), fcm_->token());

  // Send fake notification with sender id
  gcm::IncomingMessage message;
  message.data[kEmbeddedAppIdKey] = kFcmAppId;
  message.sender_id = kFcmSenderId;
  gcm_driver_.DispatchMessage(kFcmAppId, message);

  // Wait for notification callback
  EXPECT_FALSE(notifications.Take());

  // Send fake notification from topic
  message.sender_id = kFcmTopic;
  gcm_driver_.DispatchMessage(kFcmAppId, message);

  // Wait for notification callback
  EXPECT_TRUE(notifications.Take());
}

TEST_F(FcmTopicSubscriberTest, CarrierLockSubscribeTopicTwice) {
  base::test::TestFuture<Result> future;

  // Request token and subscribe with valid topic
  fcm_->SubscribeTopic(
      kFcmTopic,
      base::BindRepeating(&FcmTopicSubscriberTest::NotificationCallback,
                          base::Unretained(this)),
      future.GetCallback());
  fcm_->SubscribeTopic(
      kFcmTopic,
      base::BindRepeating(&FcmTopicSubscriberTest::NotificationCallback,
                          base::Unretained(this)),
      future.GetCallback());

  // Wait for callback
  EXPECT_EQ(Result::kHandlerBusy, future.Take());
  EXPECT_EQ(std::string(), fcm_->token());
}

TEST_F(FcmTopicSubscriberTest, CarrierLockSubscribeTopicFail) {
  base::test::TestFuture<Result> future;

  // Request token and subscribe with empty topic
  fcm_->SubscribeTopic(
      std::string(),
      base::BindRepeating(&FcmTopicSubscriberTest::NotificationCallback,
                          base::Unretained(this)),
      future.GetCallback());

  // Wait for callback
  EXPECT_EQ(Result::kInvalidInput, future.Take());
  EXPECT_NE(std::string(), fcm_->token());
}

TEST_F(FcmTopicSubscriberTest, CarrierLockGetTokenAndSubscribe) {
  base::test::TestFuture<Result> future;

  // Only request token
  fcm_->RequestToken(
      base::BindRepeating(&FcmTopicSubscriberTest::NotificationCallback,
                          base::Unretained(this)),
      future.GetCallback());

  // Wait for callback
  EXPECT_EQ(Result::kSuccess, future.Take());
  EXPECT_NE(std::string(), fcm_->token());

  // Subscribe to valid topic
  fcm_->SubscribeTopic(
      kFcmTopic,
      base::BindRepeating(&FcmTopicSubscriberTest::NotificationCallback,
                          base::Unretained(this)),
      future.GetCallback());

  // Wait for callback
  EXPECT_EQ(Result::kSuccess, future.Take());
}

}  // namespace ash::carrier_lock
