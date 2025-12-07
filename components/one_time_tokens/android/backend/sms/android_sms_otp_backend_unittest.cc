// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/android/backend/sms/android_sms_otp_backend.h"

#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_simple_task_runner.h"
#include "components/one_time_tokens/android/backend/sms/android_sms_otp_fetch_dispatcher_bridge_interface.h"
#include "components/one_time_tokens/android/backend/sms/android_sms_otp_fetch_receiver_bridge_interface.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
namespace one_time_tokens {

namespace {
using testing::AllOf;
using testing::Eq;
using testing::Field;
using testing::Optional;
using testing::Property;
using testing::Return;
using testing::StrictMock;

class MockAndroidSmsOtpFetchReceiverBridge
    : public AndroidSmsOtpFetchReceiverBridgeInterface {
 public:
  MOCK_METHOD(base::android::ScopedJavaGlobalRef<jobject>,
              GetJavaBridge,
              (),
              (const));
  MOCK_METHOD(void, SetConsumer, (base::WeakPtr<Consumer>), ());
};

class MockAndroidSmsOtpFetchDispatcherBridge
    : public AndroidSmsOtpFetchDispatcherBridgeInterface {
 public:
  MOCK_METHOD(bool, Init, (base::android::ScopedJavaGlobalRef<jobject>), ());
  MOCK_METHOD(void, RetrieveSmsOtp, (), ());
};

}  // namespace
class AndroidSmsOtpBackendTest : public testing::Test {
 protected:
  AndroidSmsOtpBackend CreateBackend(
      std::unique_ptr<AndroidSmsOtpFetchReceiverBridgeInterface>
          receiver_bridge,
      std::unique_ptr<AndroidSmsOtpFetchDispatcherBridgeInterface>
          dispatcher_bridge,
      scoped_refptr<base::SingleThreadTaskRunner> background_task_runner) {
    return AndroidSmsOtpBackend(
        base::PassKey<AndroidSmsOtpBackendTest>(), std::move(receiver_bridge),
        std::move(dispatcher_bridge), background_task_runner);
  }

  std::unique_ptr<StrictMock<MockAndroidSmsOtpFetchReceiverBridge>>
  CreateMockReceiverBridge() {
    return std::make_unique<StrictMock<MockAndroidSmsOtpFetchReceiverBridge>>();
  }

  std::unique_ptr<StrictMock<MockAndroidSmsOtpFetchDispatcherBridge>>
  CreateMockDispatcherBridge() {
    return std::make_unique<
        StrictMock<MockAndroidSmsOtpFetchDispatcherBridge>>();
  }

  scoped_refptr<base::TestSimpleTaskRunner> background_task_runner_ =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
};

TEST_F(AndroidSmsOtpBackendTest, BackendInitFails) {
  std::unique_ptr<StrictMock<MockAndroidSmsOtpFetchReceiverBridge>>
      receiver_bridge = CreateMockReceiverBridge();
  std::unique_ptr<StrictMock<MockAndroidSmsOtpFetchDispatcherBridge>>
      dispatcher_bridge = CreateMockDispatcherBridge();
  // Setup bridge
  EXPECT_CALL(*receiver_bridge, SetConsumer);
  EXPECT_CALL(*receiver_bridge, GetJavaBridge)
      .WillOnce(Return(base::android::ScopedJavaGlobalRef<jobject>()));
  // Run tasks on the background thread to trigger calls to the dispatcher
  // bridge.
  EXPECT_CALL(*dispatcher_bridge, Init).WillOnce(Return(false));
  // No fetch requests should be made if initialization failed.
  EXPECT_CALL(*dispatcher_bridge, RetrieveSmsOtp).Times(0);

  AndroidSmsOtpBackend backend =
      CreateBackend(std::move(receiver_bridge), std::move(dispatcher_bridge),
                    background_task_runner_);
  // Run the background task (dispatcher->Init)
  background_task_runner_->RunPendingTasks();
  // Run the task posted to main thread
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return backend.GetInitializationResultForTesting().has_value(); }));

  backend.RetrieveSmsOtp(base::DoNothing());
}

TEST_F(AndroidSmsOtpBackendTest, BackendInitSucceeds) {
  std::unique_ptr<StrictMock<MockAndroidSmsOtpFetchReceiverBridge>>
      receiver_bridge = CreateMockReceiverBridge();
  std::unique_ptr<StrictMock<MockAndroidSmsOtpFetchDispatcherBridge>>
      dispatcher_bridge = CreateMockDispatcherBridge();
  // Setup bridge
  EXPECT_CALL(*receiver_bridge, SetConsumer);
  EXPECT_CALL(*receiver_bridge, GetJavaBridge)
      .WillOnce(Return(base::android::ScopedJavaGlobalRef<jobject>()));
  // Run tasks on the background thread to trigger calls to the dispatcher
  // bridge.
  EXPECT_CALL(*dispatcher_bridge, Init).WillOnce(Return(true));
  // This will be called after initialization succeeds and RetrieveSmsOtp is
  // called.
  EXPECT_CALL(*dispatcher_bridge, RetrieveSmsOtp);

  AndroidSmsOtpBackend backend =
      CreateBackend(std::move(receiver_bridge), std::move(dispatcher_bridge),
                    background_task_runner_);
  // Run the background task (dispatcher->Init)
  background_task_runner_->RunPendingTasks();
  // Run the task posted to main thread
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return backend.GetInitializationResultForTesting().has_value(); }));

  backend.RetrieveSmsOtp(base::DoNothing());
}

TEST_F(AndroidSmsOtpBackendTest,
       FetchRequestReceivedBeforeBackendInitComplete) {
  std::unique_ptr<StrictMock<MockAndroidSmsOtpFetchReceiverBridge>>
      receiver_bridge = CreateMockReceiverBridge();
  std::unique_ptr<StrictMock<MockAndroidSmsOtpFetchDispatcherBridge>>
      dispatcher_bridge = CreateMockDispatcherBridge();
  // Setup bridge
  EXPECT_CALL(*receiver_bridge, SetConsumer);
  EXPECT_CALL(*receiver_bridge, GetJavaBridge)
      .WillOnce(Return(base::android::ScopedJavaGlobalRef<jobject>()));
  // No fetching should happen before initialization is complete.
  EXPECT_CALL(*dispatcher_bridge, RetrieveSmsOtp).Times(0);
  EXPECT_CALL(*dispatcher_bridge, Init).WillOnce(Return(true));
  // Fetching should happen after initialization is complete.
  EXPECT_CALL(*dispatcher_bridge, RetrieveSmsOtp);

  AndroidSmsOtpBackend backend =
      CreateBackend(std::move(receiver_bridge), std::move(dispatcher_bridge),
                    background_task_runner_);
  // Run the background task (dispatcher->Init)
  background_task_runner_->RunPendingTasks();
  // Run the task posted to main thread
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return backend.GetInitializationResultForTesting().has_value(); }));

  backend.RetrieveSmsOtp(base::DoNothing());
}

TEST_F(AndroidSmsOtpBackendTest, OtpValueFetchSucceeds) {
  std::unique_ptr<StrictMock<MockAndroidSmsOtpFetchReceiverBridge>>
      receiver_bridge = CreateMockReceiverBridge();
  std::unique_ptr<StrictMock<MockAndroidSmsOtpFetchDispatcherBridge>>
      dispatcher_bridge = CreateMockDispatcherBridge();
  // Setup bridge
  EXPECT_CALL(*receiver_bridge, SetConsumer);
  EXPECT_CALL(*receiver_bridge, GetJavaBridge)
      .WillOnce(Return(base::android::ScopedJavaGlobalRef<jobject>()));
  EXPECT_CALL(*dispatcher_bridge, Init).WillOnce(Return(true));
  // This will be called after initialization succeeds and RetrieveSmsOtp is
  // called.
  EXPECT_CALL(*dispatcher_bridge, RetrieveSmsOtp);

  AndroidSmsOtpBackend backend =
      CreateBackend(std::move(receiver_bridge), std::move(dispatcher_bridge),
                    background_task_runner_);
  // Run the background task (dispatcher->Init)
  background_task_runner_->RunPendingTasks();
  // Run the task posted to main thread
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return backend.GetInitializationResultForTesting().has_value(); }));

  base::test::TestFuture<const OtpFetchReply&> future;
  backend.RetrieveSmsOtp(future.GetCallback());
  backend.OnOtpValueRetrieved("123456");
  const OtpFetchReply& actual_result = future.Get();
  EXPECT_THAT(
      actual_result,
      AllOf(
          Field(&OtpFetchReply::otp_value,
                testing::Optional(AllOf(Property("type", &OneTimeToken::type,
                                                 Eq(OneTimeTokenType::kSmsOtp)),
                                        Property("value", &OneTimeToken::value,
                                                 Eq(std::string("123456")))))),
          Field(&OtpFetchReply::request_complete, true)));
}

TEST_F(AndroidSmsOtpBackendTest, OtpValueFetchTimesOut) {
  std::unique_ptr<StrictMock<MockAndroidSmsOtpFetchReceiverBridge>>
      receiver_bridge = CreateMockReceiverBridge();
  std::unique_ptr<StrictMock<MockAndroidSmsOtpFetchDispatcherBridge>>
      dispatcher_bridge = CreateMockDispatcherBridge();
  // Setup bridge
  EXPECT_CALL(*receiver_bridge, SetConsumer);
  EXPECT_CALL(*receiver_bridge, GetJavaBridge)
      .WillOnce(Return(base::android::ScopedJavaGlobalRef<jobject>()));
  EXPECT_CALL(*dispatcher_bridge, Init).WillOnce(Return(true));
  // This will be called after initialization succeeds and RetrieveSmsOtp is
  // called.
  EXPECT_CALL(*dispatcher_bridge, RetrieveSmsOtp);

  AndroidSmsOtpBackend backend =
      CreateBackend(std::move(receiver_bridge), std::move(dispatcher_bridge),
                    background_task_runner_);
  // Run the background task (dispatcher->Init)
  background_task_runner_->RunPendingTasks();
  // Run the task posted to main thread
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return backend.GetInitializationResultForTesting().has_value(); }));

  base::test::TestFuture<const OtpFetchReply&> future;
  backend.RetrieveSmsOtp(future.GetCallback());
  backend.OnOtpValueRetrievalError(SmsOtpRetrievalApiErrorCode::kTimeout);
  const OtpFetchReply& actual_result = future.Get();
  EXPECT_EQ(actual_result.otp_value, std::nullopt);
  EXPECT_TRUE(actual_result.request_complete);
}

TEST_F(AndroidSmsOtpBackendTest, OtpValueFetchFails) {
  std::unique_ptr<StrictMock<MockAndroidSmsOtpFetchReceiverBridge>>
      receiver_bridge = CreateMockReceiverBridge();
  std::unique_ptr<StrictMock<MockAndroidSmsOtpFetchDispatcherBridge>>
      dispatcher_bridge = CreateMockDispatcherBridge();
  // Setup bridge
  EXPECT_CALL(*receiver_bridge, SetConsumer);
  EXPECT_CALL(*receiver_bridge, GetJavaBridge)
      .WillOnce(Return(base::android::ScopedJavaGlobalRef<jobject>()));
  EXPECT_CALL(*dispatcher_bridge, Init).WillOnce(Return(true));
  // This will be called after initialization succeeds and RetrieveSmsOtp is
  // called.
  EXPECT_CALL(*dispatcher_bridge, RetrieveSmsOtp);

  AndroidSmsOtpBackend backend =
      CreateBackend(std::move(receiver_bridge), std::move(dispatcher_bridge),
                    background_task_runner_);
  // Run the background task (dispatcher->Init)
  background_task_runner_->RunPendingTasks();
  // Run the task posted to main thread
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return backend.GetInitializationResultForTesting().has_value(); }));

  base::test::TestFuture<const OtpFetchReply&> future;
  backend.RetrieveSmsOtp(future.GetCallback());
  backend.OnOtpValueRetrievalError(
      SmsOtpRetrievalApiErrorCode::kApiNotAvailable);
  const OtpFetchReply& actual_result = future.Get();
  EXPECT_EQ(actual_result.otp_value, std::nullopt);
  EXPECT_FALSE(actual_result.request_complete);
}
}  // namespace one_time_tokens
