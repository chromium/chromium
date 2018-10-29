// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/keep_alive_operation.h"

#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/components/tether/fake_ble_connection_manager.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "chromeos/components/tether/proto_test_util.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

namespace {

constexpr base::TimeDelta kKeepAliveTickleResponseTime =
    base::TimeDelta::FromSeconds(3);

class TestObserver final : public KeepAliveOperation::Observer {
 public:
  TestObserver() : has_run_callback_(false) {}

  virtual ~TestObserver() = default;

  bool has_run_callback() { return has_run_callback_; }

  base::Optional<cryptauth::RemoteDeviceRef> last_remote_device_received() {
    return last_remote_device_received_;
  }

  DeviceStatus* last_device_status_received() {
    return last_device_status_received_.get();
  }

  void OnOperationFinished(
      cryptauth::RemoteDeviceRef remote_device,
      std::unique_ptr<DeviceStatus> device_status) override {
    has_run_callback_ = true;
    last_remote_device_received_ = remote_device;
    last_device_status_received_ = std::move(device_status);
  }

 private:
  bool has_run_callback_;
  base::Optional<cryptauth::RemoteDeviceRef> last_remote_device_received_;
  std::unique_ptr<DeviceStatus> last_device_status_received_;
};

std::string CreateKeepAliveTickleString() {
  KeepAliveTickle tickle;
  return MessageWrapper(tickle).ToRawMessage();
}

std::string CreateKeepAliveTickleResponseString() {
  KeepAliveTickleResponse response;
  response.mutable_device_status()->CopyFrom(
      CreateDeviceStatusWithFakeFields());
  return MessageWrapper(response).ToRawMessage();
}

}  // namespace

class KeepAliveOperationTest : public testing::Test {
 protected:
  KeepAliveOperationTest()
      : keep_alive_tickle_string_(CreateKeepAliveTickleString()),
        test_device_(cryptauth::CreateRemoteDeviceRefListForTest(1)[0]) {}

  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(features::kMultiDeviceApi);

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();
    fake_ble_connection_manager_ = std::make_unique<FakeBleConnectionManager>();

    operation_ = base::WrapUnique(new KeepAliveOperation(
        test_device_, fake_device_sync_client_.get(),
        fake_secure_channel_client_.get(), fake_ble_connection_manager_.get()));

    test_observer_ = base::WrapUnique(new TestObserver());
    operation_->AddObserver(test_observer_.get());

    test_clock_.SetNow(base::Time::UnixEpoch());
    operation_->SetClockForTest(&test_clock_);

    operation_->Initialize();
  }

  void SimulateDeviceAuthenticationAndVerifyMessageSent() {
    operation_->OnDeviceAuthenticated(test_device_);

    // Verify that the message was sent successfully.
    std::vector<FakeBleConnectionManager::SentMessage>& sent_messages =
        fake_ble_connection_manager_->sent_messages();
    ASSERT_EQ(1u, sent_messages.size());
    EXPECT_EQ(test_device_.GetDeviceId(), sent_messages[0].device_id);
    EXPECT_EQ(keep_alive_tickle_string_, sent_messages[0].message);
  }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;
  const std::string keep_alive_tickle_string_;
  const cryptauth::RemoteDeviceRef test_device_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::SecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  base::SimpleTestClock test_clock_;
  std::unique_ptr<TestObserver> test_observer_;

  std::unique_ptr<KeepAliveOperation> operation_;

  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeepAliveOperationTest);
};

TEST_F(KeepAliveOperationTest, TestSendsKeepAliveTickleAndReceivesResponse) {
  EXPECT_FALSE(test_observer_->has_run_callback());

  SimulateDeviceAuthenticationAndVerifyMessageSent();
  EXPECT_FALSE(test_observer_->has_run_callback());

  test_clock_.Advance(kKeepAliveTickleResponseTime);

  fake_ble_connection_manager_->ReceiveMessage(
      test_device_.GetDeviceId(), CreateKeepAliveTickleResponseString());
  EXPECT_TRUE(test_observer_->has_run_callback());
  EXPECT_EQ(test_device_, test_observer_->last_remote_device_received());
  ASSERT_TRUE(test_observer_->last_device_status_received());
  EXPECT_EQ(CreateDeviceStatusWithFakeFields().SerializeAsString(),
            test_observer_->last_device_status_received()->SerializeAsString());

  histogram_tester_.ExpectTimeBucketCount(
      "InstantTethering.Performance.KeepAliveTickleResponseDuration",
      kKeepAliveTickleResponseTime, 1);
}

TEST_F(KeepAliveOperationTest, TestCannotConnect) {
  // Simulate the device failing to connect.
  fake_ble_connection_manager_->SimulateUnansweredConnectionAttempts(
      test_device_.GetDeviceId(),
      MessageTransferOperation::kMaxEmptyScansPerDevice);

  // The maximum number of connection failures has occurred.
  EXPECT_TRUE(test_observer_->has_run_callback());
  ASSERT_TRUE(test_observer_->last_remote_device_received());
  EXPECT_EQ(test_device_, test_observer_->last_remote_device_received());
  EXPECT_FALSE(test_observer_->last_device_status_received());

  histogram_tester_.ExpectTotalCount(
      "InstantTethering.Performance.KeepAliveTickleResponseDuration", 0);
}

}  // namespace tether

}  // namespace chromeos
