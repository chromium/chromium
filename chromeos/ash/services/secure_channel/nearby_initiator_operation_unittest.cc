// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/nearby_initiator_operation.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"
#include "chromeos/ash/services/secure_channel/fake_authenticated_channel.h"
#include "chromeos/ash/services/secure_channel/fake_nearby_connection_manager.h"
#include "chromeos/ash/services/secure_channel/nearby_initiator_failure_type.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

const char kTestRemoteDeviceId[] = "testRemoteDeviceId";
const char kTestLocalDeviceId[] = "testLocalDeviceId";
constexpr const ConnectionPriority kTestConnectionPriority =
    ConnectionPriority::kLow;

class SecureChannelNearbyInitiatorOperationTest : public testing::Test {
 protected:
  SecureChannelNearbyInitiatorOperationTest()
      : device_id_pair_(kTestRemoteDeviceId, kTestLocalDeviceId) {}
  SecureChannelNearbyInitiatorOperationTest(
      const SecureChannelNearbyInitiatorOperationTest&) = delete;
  SecureChannelNearbyInitiatorOperationTest& operator=(
      const SecureChannelNearbyInitiatorOperationTest&) = delete;
  ~SecureChannelNearbyInitiatorOperationTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_nearby_connection_manager_ =
        std::make_unique<FakeNearbyConnectionManager>();

    auto test_task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    operation_ = NearbyInitiatorOperation::Factory::Create(
        fake_nearby_connection_manager_.get(),
        base::BindOnce(&SecureChannelNearbyInitiatorOperationTest::
                           OnSuccessfulConnectionAttempt,
                       base::Unretained(this)),
        base::BindRepeating(&SecureChannelNearbyInitiatorOperationTest::
                                OnFailedConnectionAttempt,
                            base::Unretained(this)),
        base::BindRepeating(&SecureChannelNearbyInitiatorOperationTest::
                                OnBleDiscoveryStateChanged,
                            base::Unretained(this)),
        base::BindRepeating(&SecureChannelNearbyInitiatorOperationTest::
                                OnNearbyConnectionStateChanged,
                            base::Unretained(this)),
        base::BindRepeating(&SecureChannelNearbyInitiatorOperationTest::
                                OnSecureChannelAuthenticationStateChanged,
                            base::Unretained(this)),
        device_id_pair_, kTestConnectionPriority, test_task_runner);
    test_task_runner->RunUntilIdle();
  }

  const DeviceIdPair& device_id_pair() { return device_id_pair_; }

  void FailAttempt(NearbyInitiatorFailureType failure_type) {
    fake_nearby_connection_manager_->NotifyNearbyInitiatorFailure(
        device_id_pair_, failure_type);
    EXPECT_EQ(failure_type, failure_type_from_callback_);
  }

  FakeNearbyConnectionManager* fake_nearby_connection_manager() {
    return fake_nearby_connection_manager_.get();
  }

  AuthenticatedChannel* channel_from_callback() {
    return channel_from_callback_.get();
  }

  ConnectToDeviceOperation<NearbyInitiatorFailureType>* operation() {
    return operation_.get();
  }

 private:
  void OnSuccessfulConnectionAttempt(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel) {
    EXPECT_FALSE(channel_from_callback_);
    channel_from_callback_ = std::move(authenticated_channel);
  }

  void OnFailedConnectionAttempt(NearbyInitiatorFailureType failure_type) {
    failure_type_from_callback_ = failure_type;
  }

  void OnBleDiscoveryStateChanged(
      mojom::DiscoveryResult result,
      std::optional<mojom::DiscoveryErrorCode> error_code) {
    discovery_result_ = result;
    discovery_error_code_ = error_code;
  }

  void OnNearbyConnectionStateChanged(
      mojom::NearbyConnectionStep nearby_connection_step,
      mojom::NearbyConnectionStepResult result) {
    nearby_connection_step_ = nearby_connection_step;
    nearby_connection_step_result_ = result;
  }

  void OnSecureChannelAuthenticationStateChanged(
      mojom::SecureChannelState secure_channel_state) {
    secure_channel_state_ = secure_channel_state;
  }

  const base::test::TaskEnvironment task_environment_;

  std::unique_ptr<FakeNearbyConnectionManager> fake_nearby_connection_manager_;
  DeviceIdPair device_id_pair_;

  std::unique_ptr<AuthenticatedChannel> channel_from_callback_;
  std::optional<NearbyInitiatorFailureType> failure_type_from_callback_;

  mojom::DiscoveryResult discovery_result_;
  std::optional<mojom::DiscoveryErrorCode> discovery_error_code_;
  mojom::NearbyConnectionStep nearby_connection_step_;
  mojom::NearbyConnectionStepResult nearby_connection_step_result_;
  mojom::SecureChannelState secure_channel_state_;

  std::unique_ptr<ConnectToDeviceOperation<NearbyInitiatorFailureType>>
      operation_;
};

TEST_F(SecureChannelNearbyInitiatorOperationTest, Succeed) {
  auto fake_authenticated_channel =
      std::make_unique<FakeAuthenticatedChannel>();
  FakeAuthenticatedChannel* fake_authenticated_channel_raw =
      fake_authenticated_channel.get();

  fake_nearby_connection_manager()->NotifyNearbyInitiatorConnectionSuccess(
      device_id_pair(), std::move(fake_authenticated_channel));
  EXPECT_EQ(fake_authenticated_channel_raw, channel_from_callback());

  // The operation should no longer be present in NearbyConnectionManager.
  EXPECT_FALSE(
      fake_nearby_connection_manager()->DoesAttemptExist(device_id_pair()));
}

TEST_F(SecureChannelNearbyInitiatorOperationTest, Fail) {
  static const NearbyInitiatorFailureType all_types[] = {
      NearbyInitiatorFailureType::kConnectivityError,
      NearbyInitiatorFailureType::kAuthenticationError};

  for (const auto& failure_type : all_types) {
    FailAttempt(failure_type);
  }

  operation()->Cancel();
  EXPECT_FALSE(
      fake_nearby_connection_manager()->DoesAttemptExist(device_id_pair()));
}

}  // namespace ash::secure_channel
