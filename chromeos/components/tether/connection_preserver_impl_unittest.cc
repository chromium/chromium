// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/connection_preserver_impl.h"

#include <memory>

#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/timer/mock_timer.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/components/tether/fake_active_host.h"
#include "chromeos/components/tether/fake_ble_connection_manager.h"
#include "chromeos/components/tether/mock_tether_host_response_recorder.h"
#include "chromeos/components/tether/timer_factory.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace chromeos {

namespace tether {

namespace {

const char kWifiNetworkGuid[] = "wifiNetworkGuid";
const char kTetherNetworkGuid[] = "tetherNetworkGuid";

std::string CreateConfigurationJsonString(const std::string& guid,
                                          const std::string& type) {
  std::stringstream ss;
  ss << "{"
     << "  \"GUID\": \"" << guid << "\","
     << "  \"Type\": \"" << type << "\","
     << "  \"State\": \"" << shill::kStateReady << "\""
     << "}";
  return ss.str();
}

}  // namespace

class ConnectionPreserverImplTest : public NetworkStateTest {
 protected:
  ConnectionPreserverImplTest()
      : test_local_device_(cryptauth::RemoteDeviceRefBuilder()
                               .SetPublicKey("local device")
                               .Build()),
        test_remote_devices_(cryptauth::CreateRemoteDeviceRefListForTest(3)) {
    std::transform(
        test_remote_devices_.begin(), test_remote_devices_.end(),
        std::back_inserter(test_remote_device_ids_),
        [](const auto& remote_device) { return remote_device.GetDeviceId(); });
  }

  void SetUp() override {
    DBusThreadManager::Initialize();
    NetworkStateTest::SetUp();

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_local_device_metadata(test_local_device_);
    fake_device_sync_client_->set_synced_devices(test_remote_devices_);
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();

    fake_ble_connection_manager_ = std::make_unique<FakeBleConnectionManager>();

    fake_active_host_ = std::make_unique<FakeActiveHost>();

    previously_connected_host_ids_.clear();
    mock_tether_host_response_recorder_ =
        std::make_unique<NiceMock<MockTetherHostResponseRecorder>>();
    ON_CALL(*mock_tether_host_response_recorder_,
            GetPreviouslyConnectedHostIds())
        .WillByDefault(Invoke(
            this, &ConnectionPreserverImplTest::GetPreviouslyConnectedHostIds));

    connection_preserver_ = std::make_unique<ConnectionPreserverImpl>(
        fake_device_sync_client_.get(), fake_secure_channel_client_.get(),
        fake_ble_connection_manager_.get(), network_state_handler(),
        fake_active_host_.get(), mock_tether_host_response_recorder_.get());

    mock_timer_ = new base::MockOneShotTimer();
    connection_preserver_->SetTimerForTesting(base::WrapUnique(mock_timer_));
  }

  void TearDown() override {
    connection_preserver_.reset();

    ShutdownNetworkState();
    NetworkStateTest::TearDown();
    DBusThreadManager::Shutdown();
  }

  void SetMultiDeviceApi(bool enabled) {
    static const std::vector<base::Feature> kFeatures{
        chromeos::features::kMultiDeviceApi,
        chromeos::features::kEnableUnifiedMultiDeviceSetup};

    scoped_feature_list_.InitWithFeatures(
        (enabled ? kFeatures
                 : std::vector<base::Feature>() /* enable_features */),
        (enabled ? std::vector<base::Feature>()
                 : kFeatures /* disable_features */));
  }

  void SimulateSuccessfulHostScan_MultiDeviceApiDisabled(
      const std::string& device_id,
      bool should_remain_registered) {
    DCHECK(!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));

    base::UnguessableToken request_id = base::UnguessableToken::Create();
    fake_ble_connection_manager_->RegisterRemoteDevice(
        device_id, request_id, secure_channel::ConnectionPriority::kLow);
    EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(device_id));

    connection_preserver_->HandleSuccessfulTetherAvailabilityResponse(
        device_id);
    EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(device_id));

    fake_ble_connection_manager_->UnregisterRemoteDevice(device_id, request_id);
    EXPECT_EQ(should_remain_registered,
              fake_ble_connection_manager_->IsRegistered(device_id));
  }

  void SimulateSuccessfulHostScan_MultiDeviceApiEnabled(
      cryptauth::RemoteDeviceRef remote_device,
      bool should_remain_registered) {
    DCHECK(base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));

    // |connection_preserver_| should only grab |fake_connection_attempt| if
    // it is intended to keep the connection open.
    auto fake_connection_attempt =
        std::make_unique<secure_channel::FakeConnectionAttempt>();
    auto* fake_connection_attempt_raw = fake_connection_attempt.get();
    fake_secure_channel_client_->set_next_listen_connection_attempt(
        remote_device, test_local_device_, std::move(fake_connection_attempt));

    connection_preserver_->HandleSuccessfulTetherAvailabilityResponse(
        remote_device.GetDeviceId());

    // If |connection_preserver_| is not expected to preserve the connection, it
    // should not create a ConnectionAttempt, i.e., |next_connection_attempt|
    // should still be present.
    secure_channel::ConnectionAttempt* next_connection_attempt =
        fake_secure_channel_client_->peek_next_listen_connection_attempt(
            remote_device, test_local_device_);
    if (should_remain_registered) {
      EXPECT_FALSE(next_connection_attempt);
    } else {
      // Expect that |connection_preserver_| did not grab the ConnectionAttempt.
      EXPECT_TRUE(next_connection_attempt);
      // Clean up |next_connection_attempt| or else
      // |fake_secure_channel_client_| will fail a DCHECK when it's destroyed.
      fake_secure_channel_client_->clear_next_listen_connection_attempt(
          remote_device, test_local_device_);
      return;
    }

    auto fake_client_channel =
        std::make_unique<secure_channel::FakeClientChannel>();
    auto* fake_client_channel_raw = fake_client_channel.get();
    fake_client_channel_raw->set_destructor_callback(
        base::BindOnce(&ConnectionPreserverImplTest::OnClientChannelDestroyed,
                       base::Unretained(this), remote_device));

    fake_connection_attempt_raw->NotifyConnection(
        std::move(fake_client_channel));

    // Expect that |connection_preserver_| continues to hold on to the
    // ClientChannel until it is destroyed or the active host becomes connected.
    VerifyChannelForRemoteDeviceDestroyed(remote_device,
                                          false /* expect_destroyed */);
  }

  void VerifyChannelForRemoteDeviceDestroyed(
      cryptauth::RemoteDeviceRef remote_device,
      bool expect_destroyed) {
    if (expect_destroyed) {
      EXPECT_TRUE(remote_device_to_client_channel_destruction_count_map_
                      [remote_device]);
    } else {
      EXPECT_FALSE(remote_device_to_client_channel_destruction_count_map_
                       [remote_device]);
    }
  }

  void ConnectToWifi() {
    std::string wifi_service_path = ConfigureService(
        CreateConfigurationJsonString(kWifiNetworkGuid, shill::kTypeWifi));
  }

  std::vector<std::string> GetPreviouslyConnectedHostIds() {
    return previously_connected_host_ids_;
  }

  void OnClientChannelDestroyed(cryptauth::RemoteDeviceRef remote_device) {
    remote_device_to_client_channel_destruction_count_map_[remote_device]++;
  }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  const cryptauth::RemoteDeviceRef test_local_device_;
  const cryptauth::RemoteDeviceRefList test_remote_devices_;
  std::vector<std::string> test_remote_device_ids_;

  base::flat_map<cryptauth::RemoteDeviceRef, secure_channel::FakeClientChannel*>
      remote_device_to_fake_client_channel_map_;
  base::flat_map<cryptauth::RemoteDeviceRef, int>
      remote_device_to_client_channel_destruction_count_map_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<NiceMock<MockTetherHostResponseRecorder>>
      mock_tether_host_response_recorder_;
  base::MockOneShotTimer* mock_timer_;

  std::unique_ptr<ConnectionPreserverImpl> connection_preserver_;

  std::vector<std::string> previously_connected_host_ids_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionPreserverImplTest);
};

TEST_F(ConnectionPreserverImplTest,
       TestHandleSuccessfulTetherAvailabilityResponse_NoPreservedConnection) {
  SetMultiDeviceApi(false /* enabled */);
  SimulateSuccessfulHostScan_MultiDeviceApiDisabled(
      test_remote_device_ids_[0], true /* should_remain_registered */);
}

TEST_F(
    ConnectionPreserverImplTest,
    TestHandleSuccessfulTetherAvailabilityResponse_NoPreservedConnection_MultiDeviceApiEnabled) {
  SetMultiDeviceApi(true /* enabled */);

  SimulateSuccessfulHostScan_MultiDeviceApiEnabled(
      test_remote_devices_[0], true /* should_remain_registered */);
}

TEST_F(ConnectionPreserverImplTest,
       TestHandleSuccessfulTetherAvailabilityResponse_HasInternet) {
  SetMultiDeviceApi(false /* enabled */);
  ConnectToWifi();

  SimulateSuccessfulHostScan_MultiDeviceApiDisabled(
      test_remote_device_ids_[0], false /* should_remain_registered */);
}

TEST_F(
    ConnectionPreserverImplTest,
    TestHandleSuccessfulTetherAvailabilityResponse_HasInternet_MultiDeviceApiEnabled) {
  SetMultiDeviceApi(true /* enabled */);

  ConnectToWifi();

  SimulateSuccessfulHostScan_MultiDeviceApiEnabled(
      test_remote_devices_[0], false /* should_remain_registered */);
}

TEST_F(
    ConnectionPreserverImplTest,
    TestHandleSuccessfulTetherAvailabilityResponse_PreservedConnectionExists_NoPreviouslyConnectedHosts) {
  SetMultiDeviceApi(false /* enabled */);
  SimulateSuccessfulHostScan_MultiDeviceApiDisabled(
      test_remote_device_ids_[0], true /* should_remain_registered */);
  SimulateSuccessfulHostScan_MultiDeviceApiDisabled(
      test_remote_device_ids_[1], true /* should_remain_registered */);
  EXPECT_FALSE(
      fake_ble_connection_manager_->IsRegistered(test_remote_device_ids_[0]));
}

TEST_F(
    ConnectionPreserverImplTest,
    TestHandleSuccessfulTetherAvailabilityResponse_PreservedConnectionExists_NoPreviouslyConnectedHosts_MultiDeviceApiEnabled) {
  SetMultiDeviceApi(true /* enabled */);

  SimulateSuccessfulHostScan_MultiDeviceApiEnabled(
      test_remote_devices_[0], true /* should_remain_registered */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[0],
                                        false /* expect_destroyed */);

  SimulateSuccessfulHostScan_MultiDeviceApiEnabled(
      test_remote_devices_[1], true /* should_remain_registered */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[0],
                                        true /* expect_destroyed */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[1],
                                        false /* expect_destroyed */);
}

TEST_F(ConnectionPreserverImplTest,
       TestHandleSuccessfulTetherAvailabilityResponse_TimesOut) {
  SetMultiDeviceApi(false /* enabled */);
  SimulateSuccessfulHostScan_MultiDeviceApiDisabled(
      test_remote_device_ids_[0], true /* should_remain_registered */);

  mock_timer_->Fire();
  EXPECT_FALSE(
      fake_ble_connection_manager_->IsRegistered(test_remote_device_ids_[0]));
}

TEST_F(
    ConnectionPreserverImplTest,
    TestHandleSuccessfulTetherAvailabilityResponse_TimesOut_MultiDeviceApiEnabled) {
  SetMultiDeviceApi(true /* enabled */);

  SimulateSuccessfulHostScan_MultiDeviceApiEnabled(
      test_remote_devices_[0], true /* should_remain_registered */);

  mock_timer_->Fire();
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[0],
                                        true /* expect_destroyed */);
}

TEST_F(ConnectionPreserverImplTest,
       TestHandleSuccessfulTetherAvailabilityResponse_PreserverDestroyed) {
  SetMultiDeviceApi(false /* enabled */);
  SimulateSuccessfulHostScan_MultiDeviceApiDisabled(
      test_remote_device_ids_[0], true /* should_remain_registered */);

  connection_preserver_.reset();
  EXPECT_FALSE(
      fake_ble_connection_manager_->IsRegistered(test_remote_device_ids_[0]));
}

TEST_F(
    ConnectionPreserverImplTest,
    TestHandleSuccessfulTetherAvailabilityResponse_PreserverDestroyed_MultiDeviceApiEnabled) {
  SetMultiDeviceApi(true /* enabled */);

  SimulateSuccessfulHostScan_MultiDeviceApiEnabled(
      test_remote_devices_[0], true /* should_remain_registered */);

  connection_preserver_.reset();
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[0],
                                        true /* expect_destroyed */);
}

TEST_F(
    ConnectionPreserverImplTest,
    TestHandleSuccessfulTetherAvailabilityResponse_ActiveHostBecomesConnected) {
  SetMultiDeviceApi(false /* enabled */);
  SimulateSuccessfulHostScan_MultiDeviceApiDisabled(
      test_remote_device_ids_[0], true /* should_remain_registered */);

  fake_active_host_->SetActiveHostConnecting(test_remote_device_ids_[0],
                                             kTetherNetworkGuid);
  fake_active_host_->SetActiveHostConnected(
      test_remote_device_ids_[0], kTetherNetworkGuid, kWifiNetworkGuid);
  EXPECT_FALSE(
      fake_ble_connection_manager_->IsRegistered(test_remote_device_ids_[0]));
}

TEST_F(
    ConnectionPreserverImplTest,
    TestHandleSuccessfulTetherAvailabilityResponse_ActiveHostBecomesConnected_MultiDeviceApiEnabled) {
  SetMultiDeviceApi(true /* enabled */);

  SimulateSuccessfulHostScan_MultiDeviceApiEnabled(
      test_remote_devices_[0], true /* should_remain_registered */);

  fake_active_host_->SetActiveHostConnecting(test_remote_device_ids_[0],
                                             kTetherNetworkGuid);
  fake_active_host_->SetActiveHostConnected(
      test_remote_device_ids_[0], kTetherNetworkGuid, kWifiNetworkGuid);

  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[0],
                                        true /* expect_destroyed */);
}

TEST_F(
    ConnectionPreserverImplTest,
    TestHandleSuccessfulTetherAvailabilityResponse_PreviouslyConnectedHostsExist) {
  SetMultiDeviceApi(false /* enabled */);

  // |test_remote_device_ids_[0]| is the most recently connected device, and
  // should be preferred over any other device.
  previously_connected_host_ids_.push_back(test_remote_device_ids_[0]);
  previously_connected_host_ids_.push_back(test_remote_device_ids_[1]);

  SimulateSuccessfulHostScan_MultiDeviceApiDisabled(
      test_remote_device_ids_[2], true /* should_remain_registered */);

  SimulateSuccessfulHostScan_MultiDeviceApiDisabled(
      test_remote_device_ids_[1], true /* should_remain_registered */);
  EXPECT_FALSE(
      fake_ble_connection_manager_->IsRegistered(test_remote_device_ids_[2]));

  SimulateSuccessfulHostScan_MultiDeviceApiDisabled(
      test_remote_device_ids_[0], true /* should_remain_registered */);
  EXPECT_FALSE(
      fake_ble_connection_manager_->IsRegistered(test_remote_device_ids_[1]));

  SimulateSuccessfulHostScan_MultiDeviceApiDisabled(
      test_remote_device_ids_[1], false /* should_remain_registered */);
  EXPECT_TRUE(
      fake_ble_connection_manager_->IsRegistered(test_remote_device_ids_[0]));

  SimulateSuccessfulHostScan_MultiDeviceApiDisabled(
      test_remote_device_ids_[2], false /* should_remain_registered */);
  EXPECT_TRUE(
      fake_ble_connection_manager_->IsRegistered(test_remote_device_ids_[0]));
}

TEST_F(
    ConnectionPreserverImplTest,
    TestHandleSuccessfulTetherAvailabilityResponse_PreviouslyConnectedHostsExist_MultiDeviceApiEnabled) {
  SetMultiDeviceApi(true /* enabled */);

  // |test_remote_device_ids_[0]| is the most recently connected device, and
  // should be preferred over any other device.
  previously_connected_host_ids_.push_back(test_remote_device_ids_[0]);
  previously_connected_host_ids_.push_back(test_remote_device_ids_[1]);

  SimulateSuccessfulHostScan_MultiDeviceApiEnabled(
      test_remote_devices_[2], true /* should_remain_registered */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[2],
                                        false /* expect_destroyed */);

  SimulateSuccessfulHostScan_MultiDeviceApiEnabled(
      test_remote_devices_[1], true /* should_remain_registered */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[2],
                                        true /* expect_destroyed */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[1],
                                        false /* expect_destroyed */);

  SimulateSuccessfulHostScan_MultiDeviceApiEnabled(
      test_remote_devices_[0], true /* should_remain_registered */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[1],
                                        true /* expect_destroyed */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[0],
                                        false /* expect_destroyed */);

  SimulateSuccessfulHostScan_MultiDeviceApiEnabled(
      test_remote_devices_[1], false /* should_remain_registered */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[0],
                                        false /* expect_destroyed */);

  SimulateSuccessfulHostScan_MultiDeviceApiEnabled(
      test_remote_devices_[2], false /* should_remain_registered */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[0],
                                        false /* expect_destroyed */);
}

}  // namespace tether

}  // namespace chromeos
