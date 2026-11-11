// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/android/backend/sms/android_sms_otp_backend.h"

#include "base/test/gmock_expected_support.h"
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

  AndroidSmsOtpBackend CreateBackendAndSetupBridges(
      bool init_success,
      int expected_retrieve_calls = 0) {
    auto receiver_bridge = CreateMockReceiverBridge();
    auto dispatcher_bridge = CreateMockDispatcherBridge();
    EXPECT_CALL(*receiver_bridge, SetConsumer);
    EXPECT_CALL(*receiver_bridge, GetJavaBridge)
        .WillOnce(Return(base::android::ScopedJavaGlobalRef<jobject>()));
    EXPECT_CALL(*dispatcher_bridge, Init).WillOnce(Return(init_success));
    EXPECT_CALL(*dispatcher_bridge, RetrieveSmsOtp)
        .Times(expected_retrieve_calls);
    return CreateBackend(std::move(receiver_bridge),
                         std::move(dispatcher_bridge), background_task_runner_);
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
  AndroidSmsOtpBackend backend = CreateBackendAndSetupBridges(
      /*init_success=*/false, /*expected_retrieve_calls=*/0);
  // Run the background task (dispatcher->Init)
  background_task_runner_->RunPendingTasks();
  // Run the task posted to main thread
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return backend.GetInitializationResultForTesting().has_value(); }));

  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  backend.RetrieveSmsOtp(future.GetCallback());
  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            OneTimeTokenRetrievalError::kSmsOtpBackendInitializationFailed);
}

TEST_F(AndroidSmsOtpBackendTest, BackendInitSucceeds) {
  AndroidSmsOtpBackend backend = CreateBackendAndSetupBridges(
      /*init_success=*/true, /*expected_retrieve_calls=*/1);
  // Run the background task (dispatcher->Init)
  background_task_runner_->RunPendingTasks();
  // Run the task posted to main thread
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return backend.GetInitializationResultForTesting().has_value(); }));

  backend.RetrieveSmsOtp(base::DoNothing());
}

TEST_F(AndroidSmsOtpBackendTest,
       FetchRequestsReceivedBeforeBackendInitComplete) {
  AndroidSmsOtpBackend backend = CreateBackendAndSetupBridges(
      /*init_success=*/true, /*expected_retrieve_calls=*/2);

  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future1;
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future2;
  backend.RetrieveSmsOtp(future1.GetCallback());
  backend.RetrieveSmsOtp(future2.GetCallback());
  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  background_task_runner_->RunPendingTasks();
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return backend.GetInitializationResultForTesting().has_value(); }));
  background_task_runner_->RunPendingTasks();

  backend.OnOtpValueRetrieved("123456");
  backend.OnOtpValueRetrievalError(SmsOtpRetrievalApiError::kTimeout);
  EXPECT_THAT(future1.Get(),
              base::test::ValueIs(Property(&OneTimeToken::value, "123456")));
  EXPECT_THAT(
      future2.Get(),
      base::test::ErrorIs(OneTimeTokenRetrievalError::kSmsOtpBackendTimeout));
}

TEST_F(AndroidSmsOtpBackendTest, FetchRequestsReceivedBeforeBackendInitFails) {
  AndroidSmsOtpBackend backend = CreateBackendAndSetupBridges(
      /*init_success=*/false, /*expected_retrieve_calls=*/0);

  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future1;
  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future2;
  backend.RetrieveSmsOtp(future1.GetCallback());
  backend.RetrieveSmsOtp(future2.GetCallback());
  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  background_task_runner_->RunPendingTasks();
  EXPECT_THAT(
      future1.Get(),
      base::test::ErrorIs(
          OneTimeTokenRetrievalError::kSmsOtpBackendInitializationFailed));
  EXPECT_THAT(
      future2.Get(),
      base::test::ErrorIs(
          OneTimeTokenRetrievalError::kSmsOtpBackendInitializationFailed));
}

TEST_F(AndroidSmsOtpBackendTest, OtpValueFetchSucceeds) {
  AndroidSmsOtpBackend backend = CreateBackendAndSetupBridges(
      /*init_success=*/true, /*expected_retrieve_calls=*/1);
  // Run the background task (dispatcher->Init)
  background_task_runner_->RunPendingTasks();
  // Run the task posted to main thread
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return backend.GetInitializationResultForTesting().has_value(); }));

  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  backend.RetrieveSmsOtp(future.GetCallback());
  backend.OnOtpValueRetrieved("123456");
  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>&
      actual_result = future.Get();
  EXPECT_THAT(actual_result.value().value(), testing::Eq("123456"));
}

TEST_F(AndroidSmsOtpBackendTest, OtpValueFetchTimesOut) {
  AndroidSmsOtpBackend backend = CreateBackendAndSetupBridges(
      /*init_success=*/true, /*expected_retrieve_calls=*/1);
  // Run the background task (dispatcher->Init)
  background_task_runner_->RunPendingTasks();
  // Run the task posted to main thread
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return backend.GetInitializationResultForTesting().has_value(); }));

  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  backend.RetrieveSmsOtp(future.GetCallback());
  backend.OnOtpValueRetrievalError(SmsOtpRetrievalApiError::kTimeout);
  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>&
      actual_result = future.Get();
  ASSERT_FALSE(actual_result.has_value());
  EXPECT_EQ(actual_result.error(),
            OneTimeTokenRetrievalError::kSmsOtpBackendTimeout);
}

TEST_F(AndroidSmsOtpBackendTest, OtpValueFetchFails) {
  AndroidSmsOtpBackend backend = CreateBackendAndSetupBridges(
      /*init_success=*/true, /*expected_retrieve_calls=*/1);
  // Run the background task (dispatcher->Init)
  background_task_runner_->RunPendingTasks();
  // Run the task posted to main thread
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return backend.GetInitializationResultForTesting().has_value(); }));

  base::test::TestFuture<
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      future;
  backend.RetrieveSmsOtp(future.GetCallback());
  backend.OnOtpValueRetrievalError(SmsOtpRetrievalApiError::kApiNotAvailable);
  const base::expected<OneTimeToken, OneTimeTokenRetrievalError>&
      actual_result = future.Get();
  ASSERT_FALSE(actual_result.has_value());
  EXPECT_EQ(actual_result.error(),
            OneTimeTokenRetrievalError::kSmsOtpBackendApiNotAvailable);
}
}  // namespace one_time_tokens
