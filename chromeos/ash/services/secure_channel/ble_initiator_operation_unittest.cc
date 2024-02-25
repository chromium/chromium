// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/ble_initiator_operation.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/ash/services/secure_channel/ble_initiator_failure_type.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"
#include "chromeos/ash/services/secure_channel/fake_authenticated_channel.h"
#include "chromeos/ash/services/secure_channel/fake_ble_connection_manager.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

const char kTestRemoteDeviceId[] = "testRemoteDeviceId";
const char kTestLocalDeviceId[] = "testLocalDeviceId";
constexpr const ConnectionPriority kTestConnectionPriority =
    ConnectionPriority::kLow;

class SecureChannelBleInitiatorOperationTest : public testing::Test {
 public:
  SecureChannelBleInitiatorOperationTest(
      const SecureChannelBleInitiatorOperationTest&) = delete;
  SecureChannelBleInitiatorOperationTest& operator=(
      const SecureChannelBleInitiatorOperationTest&) = delete;

 protected:
  SecureChannelBleInitiatorOperationTest()
      : device_id_pair_(kTestRemoteDeviceId, kTestLocalDeviceId) {}

  ~SecureChannelBleInitiatorOperationTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_ble_connection_manager_ = std::make_unique<FakeBleConnectionManager>();

    auto test_task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    operation_ = BleInitiatorOperation::Factory::Create(
        fake_ble_connection_manager_.get(),
        base::BindOnce(&SecureChannelBleInitiatorOperationTest::
                           OnSuccessfulConnectionAttempt,
                       base::Unretained(this)),
        base::BindRepeating(
            &SecureChannelBleInitiatorOperationTest::OnFailedConnectionAttempt,
            base::Unretained(this)),
        device_id_pair_, kTestConnectionPriority, test_task_runner);
    test_task_runner->RunUntilIdle();

    EXPECT_EQ(kTestConnectionPriority,
              fake_ble_connection_manager_->GetPriorityForAttempt(
                  device_id_pair_, ConnectionRole::kInitiatorRole));
  }

  const DeviceIdPair& device_id_pair() { return device_id_pair_; }

  void FailAttempt(BleInitiatorFailureType failure_type) {
    fake_ble_connection_manager_->NotifyBleInitiatorFailure(device_id_pair_,
                                                            failure_type);
    EXPECT_EQ(failure_type, failure_type_from_callback_);
  }

  void CompleteAttemptSuccessfully() {
    auto fake_authenticated_channel =
        std::make_unique<FakeAuthenticatedChannel>();
    FakeAuthenticatedChannel* fake_authenticated_channel_raw =
        fake_authenticated_channel.get();

    fake_ble_connection_manager_->NotifyConnectionSuccess(
        device_id_pair(), ConnectionRole::kInitiatorRole,
        std::move(fake_authenticated_channel));
    EXPECT_EQ(fake_authenticated_channel_raw, channel_from_callback_.get());

    // The operation should no longer be present in BleConnectionManager.
    EXPECT_FALSE(fake_ble_connection_manager()->DoesAttemptExist(
        device_id_pair(), ConnectionRole::kInitiatorRole));
  }

  FakeBleConnectionManager* fake_ble_connection_manager() {
    return fake_ble_connection_manager_.get();
  }

  ConnectToDeviceOperation<BleInitiatorFailureType>* operation() {
    return operation_.get();
  }

 private:
  void OnSuccessfulConnectionAttempt(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel) {
    EXPECT_FALSE(channel_from_callback_);
    channel_from_callback_ = std::move(authenticated_channel);
  }

  void OnFailedConnectionAttempt(BleInitiatorFailureType failure_type) {
    failure_type_from_callback_ = failure_type;
  }

  const base::test::TaskEnvironment task_environment_;

  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  DeviceIdPair device_id_pair_;

  std::unique_ptr<AuthenticatedChannel> channel_from_callback_;
  std::optional<BleInitiatorFailureType> failure_type_from_callback_;

  std::unique_ptr<ConnectToDeviceOperation<BleInitiatorFailureType>> operation_;
};

TEST_F(SecureChannelBleInitiatorOperationTest, UpdateThenFail) {
  operation()->UpdateConnectionPriority(ConnectionPriority::kMedium);
  EXPECT_EQ(ConnectionPriority::kMedium,
            fake_ble_connection_manager()->GetPriorityForAttempt(
                device_id_pair(), ConnectionRole::kInitiatorRole));

  static const BleInitiatorFailureType all_types[] = {
      BleInitiatorFailureType::kAuthenticationError,
      BleInitiatorFailureType::kGattConnectionError,
      BleInitiatorFailureType::kInterruptedByHigherPriorityConnectionAttempt,
      BleInitiatorFailureType::kTimeoutContactingRemoteDevice,
      BleInitiatorFailureType::kCouldNotGenerateAdvertisement};

  for (const auto& failure_type : all_types) {
    FailAttempt(failure_type);

    // After failure, the attempt should still be present in
    // BleConnectionManager.
    EXPECT_EQ(ConnectionPriority::kMedium,
              fake_ble_connection_manager()->GetPriorityForAttempt(
                  device_id_pair(), ConnectionRole::kInitiatorRole));
  }

  operation()->Cancel();
  EXPECT_FALSE(fake_ble_connection_manager()->DoesAttemptExist(
      device_id_pair(), ConnectionRole::kInitiatorRole));
}

TEST_F(SecureChannelBleInitiatorOperationTest, UpdateThenSucceed) {
  operation()->UpdateConnectionPriority(ConnectionPriority::kMedium);
  EXPECT_EQ(ConnectionPriority::kMedium,
            fake_ble_connection_manager()->GetPriorityForAttempt(
                device_id_pair(), ConnectionRole::kInitiatorRole));
  CompleteAttemptSuccessfully();
}

}  // namespace ash::secure_channel
