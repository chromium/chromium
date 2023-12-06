// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/connect_to_device_operation_base.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"
#include "chromeos/ash/services/secure_channel/fake_authenticated_channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

const char kTestRemoteDeviceId[] = "testRemoteDeviceId";
const char kTestLocalDeviceId[] = "testLocalDeviceId";

// Since ConnectToDeviceOperationBase is templatized, a concrete implementation
// is needed for its test.
class TestConnectToDeviceOperation
    : public ConnectToDeviceOperationBase<std::string> {
 public:
  static std::unique_ptr<TestConnectToDeviceOperation> Create(
      ConnectToDeviceOperation<std::string>::ConnectionSuccessCallback
          success_callback,
      const ConnectToDeviceOperation<std::string>::ConnectionFailedCallback&
          failure_callback,
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority,
      bool should_attempt_connection_synchronously) {
    auto test_task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    auto operation = base::WrapUnique(new TestConnectToDeviceOperation(
        std::move(success_callback), std::move(failure_callback),
        device_id_pair, connection_priority, test_task_runner));

    if (should_attempt_connection_synchronously)
      test_task_runner->RunUntilIdle();

    return operation;
  }

  ~TestConnectToDeviceOperation() override = default;

  void RunPendingTasks() { test_task_runner_->RunUntilIdle(); }

  bool has_attempted_connection() const { return has_attempted_connection_; }
  bool has_canceled_connection() const { return has_canceled_connection_; }
  const std::optional<ConnectionPriority>& current_connection_priority() {
    return current_connection_priority_;
  }

  // ConnectToDeviceOperationBase<std::string>:
  void PerformAttemptConnectionToDevice(
      ConnectionPriority connection_priority) override {
    has_attempted_connection_ = true;
    current_connection_priority_ = connection_priority;
  }

  void PerformCancellation() override {
    has_canceled_connection_ = true;
    current_connection_priority_.reset();
  }

  void PerformUpdateConnectionPriority(
      ConnectionPriority connection_priority) override {
    current_connection_priority_ = connection_priority;
  }

  // Make On{Successful|Failed}ConnectionAttempt() public for testing.
  using ConnectToDeviceOperation<std::string>::OnSuccessfulConnectionAttempt;
  using ConnectToDeviceOperation<std::string>::OnFailedConnectionAttempt;

 private:
  TestConnectToDeviceOperation(
      ConnectToDeviceOperation<std::string>::ConnectionSuccessCallback
          success_callback,
      const ConnectToDeviceOperation<std::string>::ConnectionFailedCallback&
          failure_callback,
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority,
      scoped_refptr<base::TestSimpleTaskRunner> test_task_runner)
      : ConnectToDeviceOperationBase<std::string>(std::move(success_callback),
                                                  std::move(failure_callback),
                                                  device_id_pair,
                                                  connection_priority,
                                                  test_task_runner),
        test_task_runner_(test_task_runner) {}

  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner_;
  bool has_attempted_connection_ = false;
  bool has_canceled_connection_ = false;
  std::optional<ConnectionPriority> current_connection_priority_;
};

}  // namespace

class SecureChannelConnectToDeviceOperationBaseTest : public testing::Test {
 public:
  SecureChannelConnectToDeviceOperationBaseTest(
      const SecureChannelConnectToDeviceOperationBaseTest&) = delete;
  SecureChannelConnectToDeviceOperationBaseTest& operator=(
      const SecureChannelConnectToDeviceOperationBaseTest&) = delete;

 protected:
  SecureChannelConnectToDeviceOperationBaseTest()
      : test_device_id_pair_(kTestRemoteDeviceId, kTestLocalDeviceId) {}

  ~SecureChannelConnectToDeviceOperationBaseTest() override = default;

  void CreateOperation(ConnectionPriority connection_priority,
                       bool should_attempt_connection_synchronously = true) {
    test_operation_ = TestConnectToDeviceOperation::Create(
        base::BindOnce(&SecureChannelConnectToDeviceOperationBaseTest::
                           OnSuccessfulConnectionAttempt,
                       base::Unretained(this)),
        base::BindRepeating(&SecureChannelConnectToDeviceOperationBaseTest::
                                OnFailedConnectionAttempt,
                            base::Unretained(this)),
        test_device_id_pair_, connection_priority,
        should_attempt_connection_synchronously);
    EXPECT_EQ(should_attempt_connection_synchronously,
              test_operation_->has_attempted_connection());
  }

  TestConnectToDeviceOperation* test_operation() {
    return test_operation_.get();
  }

  const AuthenticatedChannel* last_authenticated_channel() const {
    return last_authenticated_channel_.get();
  }

  const std::string& last_failure_detail() const {
    return last_failure_detail_;
  }

 private:
  void OnSuccessfulConnectionAttempt(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel) {
    last_authenticated_channel_ = std::move(authenticated_channel);
  }

  void OnFailedConnectionAttempt(std::string failure_detail) {
    last_failure_detail_ = failure_detail;
  }

  base::test::TaskEnvironment task_environment_;
  const DeviceIdPair test_device_id_pair_;

  std::unique_ptr<AuthenticatedChannel> last_authenticated_channel_;
  std::string last_failure_detail_;

  std::unique_ptr<TestConnectToDeviceOperation> test_operation_;
};

TEST_F(SecureChannelConnectToDeviceOperationBaseTest, Success) {
  CreateOperation(ConnectionPriority::kLow);
  EXPECT_EQ(ConnectionPriority::kLow, test_operation()->connection_priority());

  test_operation()->UpdateConnectionPriority(ConnectionPriority::kMedium);
  EXPECT_EQ(ConnectionPriority::kMedium,
            test_operation()->connection_priority());

  test_operation()->UpdateConnectionPriority(ConnectionPriority::kHigh);
  EXPECT_EQ(ConnectionPriority::kHigh, test_operation()->connection_priority());

  auto fake_authenticated_channel =
      std::make_unique<FakeAuthenticatedChannel>();
  auto* fake_authenticated_channel_raw = fake_authenticated_channel.get();
  test_operation()->OnSuccessfulConnectionAttempt(
      std::move(fake_authenticated_channel));

  EXPECT_EQ(fake_authenticated_channel_raw, last_authenticated_channel());
}

TEST_F(SecureChannelConnectToDeviceOperationBaseTest, Failure) {
  CreateOperation(ConnectionPriority::kLow);

  test_operation()->OnFailedConnectionAttempt("failureReason1");
  EXPECT_EQ("failureReason1", last_failure_detail());

  test_operation()->OnFailedConnectionAttempt("failureReason2");
  EXPECT_EQ("failureReason2", last_failure_detail());

  test_operation()->Cancel();
}

TEST_F(SecureChannelConnectToDeviceOperationBaseTest, Cancelation) {
  CreateOperation(ConnectionPriority::kLow);
  test_operation()->Cancel();
  EXPECT_TRUE(test_operation()->has_canceled_connection());
}

TEST_F(SecureChannelConnectToDeviceOperationBaseTest,
       UpdateConnectionPriorityBeforeAttemptStarted) {
  CreateOperation(ConnectionPriority::kLow,
                  false /* should_attempt_connection_synchronously */);

  // Update the connection priority, then run pending tasks; the pending task
  // should start the attempt.
  test_operation()->UpdateConnectionPriority(ConnectionPriority::kMedium);
  EXPECT_FALSE(test_operation()->has_attempted_connection());
  test_operation()->RunPendingTasks();
  EXPECT_TRUE(test_operation()->has_attempted_connection());
  EXPECT_EQ(ConnectionPriority::kMedium,
            test_operation()->current_connection_priority());

  test_operation()->Cancel();
}

TEST_F(SecureChannelConnectToDeviceOperationBaseTest,
       CancelBeforeAttemptStarted) {
  CreateOperation(ConnectionPriority::kLow,
                  false /* should_attempt_connection_synchronously */);

  // Update the connection priority, cancel the attempt, then run pending tasks;
  // the pending task should try to start the attempt but should fail since the
  // task had already been canceled.
  test_operation()->UpdateConnectionPriority(ConnectionPriority::kMedium);
  EXPECT_FALSE(test_operation()->has_attempted_connection());
  test_operation()->Cancel();
  EXPECT_FALSE(test_operation()->has_attempted_connection());
  test_operation()->RunPendingTasks();
  EXPECT_FALSE(test_operation()->has_attempted_connection());
}

}  // namespace ash::secure_channel
