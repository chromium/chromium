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
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::carrier_lock {

namespace {

const char kEmbeddedAppIdKey[] = "gcmb";
const char kFcmAppId[] = "com.google.chromeos.carrier_lock";
const char kFcmSenderId[] = "1067228791894";
const char kFcmTopic[] = "/topics/testtopic";
const uint64_t kTestAndroidSecret = 1234;
const uint64_t kTestAndroidId = 1234;
const char kFcmUrl[] = "https://android.clients.google.com/c2dm/register3";

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
    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    fcm_ = std::make_unique<FcmTopicSubscriberImpl>(
        &gcm_driver_, kFcmAppId, kFcmSenderId, shared_factory_);
    fcm_->android_id_ = kTestAndroidId;
    fcm_->android_secret_ = kTestAndroidSecret;
    fcm_->set_is_testing(true);
    fcm_->Initialize(base::BindRepeating(
        &FcmTopicSubscriberTest::NotificationCallback, base::Unretained(this)));
  }

  void TearDown() override { fcm_.reset(); }

  std::unique_ptr<FcmTopicSubscriberImpl> fcm_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  instance_id::FakeGCMDriverForInstanceID gcm_driver_;
};

TEST_F(FcmTopicSubscriberTest, CarrierLockSubscribeTopicSuccess) {
  base::test::TestFuture<Result> future;

  // Request token and subscribe with valid topic
  fcm_->SubscribeTopic(
      kFcmTopic,
      future.GetCallback());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(kFcmUrl), network::URLLoaderCompletionStatus(net::OK),
      network::CreateURLResponseHead(net::HTTP_OK), std::string("{}")));

  // Wait for callback
  EXPECT_EQ(Result::kSuccess, future.Get());
  EXPECT_NE(std::string(), fcm_->token());
}

TEST_F(FcmTopicSubscriberTest, CarrierLockTestNotifications) {
  base::test::TestFuture<Result> future;
  base::test::RepeatingTestFuture<bool> notifications;

  // Request token and subscribe with valid topic
  fcm_->Initialize(notifications.GetCallback());
  fcm_->SubscribeTopic(kFcmTopic, future.GetCallback());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(kFcmUrl), network::URLLoaderCompletionStatus(net::OK),
      network::CreateURLResponseHead(net::HTTP_OK), std::string("{}")));

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
      future.GetCallback());
  fcm_->SubscribeTopic(
      kFcmTopic,
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
      future.GetCallback());

  // Wait for callback
  EXPECT_EQ(Result::kInvalidInput, future.Take());
  EXPECT_NE(std::string(), fcm_->token());
}

TEST_F(FcmTopicSubscriberTest, CarrierLockGetTokenAndSubscribe) {
  base::test::TestFuture<Result> future;

  // Only request token
  fcm_->RequestToken(
      future.GetCallback());

  // Wait for callback
  EXPECT_EQ(Result::kSuccess, future.Take());
  EXPECT_NE(std::string(), fcm_->token());

  // Subscribe to valid topic
  fcm_->SubscribeTopic(
      kFcmTopic,
      future.GetCallback());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(kFcmUrl), network::URLLoaderCompletionStatus(net::OK),
      network::CreateURLResponseHead(net::HTTP_OK), std::string("{}")));

  // Wait for callback
  EXPECT_EQ(Result::kSuccess, future.Take());
}

}  // namespace ash::carrier_lock
