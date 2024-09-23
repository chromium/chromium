// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/connection_preserver_impl.h"

#include <memory>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/task_environment.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/tether/fake_active_host.h"
#include "chromeos/ash/components/tether/fake_host_connection.h"
#include "chromeos/ash/components/tether/mock_tether_host_response_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace ash::tether {

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

class ConnectionPreserverImplTest : public testing::Test {
 public:
  ConnectionPreserverImplTest(const ConnectionPreserverImplTest&) = delete;
  ConnectionPreserverImplTest& operator=(const ConnectionPreserverImplTest&) =
      delete;

 protected:
  ConnectionPreserverImplTest()
      : test_remote_devices_(multidevice::CreateRemoteDeviceRefListForTest(3)) {
    base::ranges::transform(test_remote_devices_,
                            std::back_inserter(test_remote_device_ids_),
                            &multidevice::RemoteDeviceRef::GetDeviceId);
  }

  void SetUp() override {
    fake_host_connection_factory_ =
        std::make_unique<FakeHostConnection::Factory>();
    fake_active_host_ = std::make_unique<FakeActiveHost>();

    previously_connected_host_ids_.clear();
    mock_tether_host_response_recorder_ =
        std::make_unique<NiceMock<MockTetherHostResponseRecorder>>();
    ON_CALL(*mock_tether_host_response_recorder_,
            GetPreviouslyConnectedHostIds())
        .WillByDefault(Invoke(
            this, &ConnectionPreserverImplTest::GetPreviouslyConnectedHostIds));

    connection_preserver_ = std::make_unique<ConnectionPreserverImpl>(
        fake_host_connection_factory_.get(), helper_.network_state_handler(),
        fake_active_host_.get(), mock_tether_host_response_recorder_.get());

    mock_timer_ = new base::MockOneShotTimer();
    connection_preserver_->SetTimerForTesting(
        base::WrapUnique(mock_timer_.get()));
  }

  void TearDown() override { connection_preserver_.reset(); }

  void SimulateSuccessfulHostScan(multidevice::RemoteDeviceRef remote_device,
                                  bool should_remain_registered) {
    fake_host_connection_factory_->SetupConnectionAttempt(
        TetherHost(remote_device));

    connection_preserver_->HandleSuccessfulTetherAvailabilityResponse(
        remote_device.GetDeviceId());

    if (should_remain_registered) {
      EXPECT_EQ(fake_host_connection_factory_->GetPendingConnectionAttempt(
                    remote_device.GetDeviceId()),
                nullptr);
    } else {
      EXPECT_TRUE(fake_host_connection_factory_->GetPendingConnectionAttempt(
          remote_device.GetDeviceId()));
      return;
    }

    // Expect that |connection_preserver_| continues to hold on to the
    // ClientChannel until it is destroyed or the active host becomes connected.
    VerifyChannelForRemoteDeviceDestroyed(remote_device,
                                          false /* expect_destroyed */);
  }

  void VerifyChannelForRemoteDeviceDestroyed(
      multidevice::RemoteDeviceRef remote_device,
      bool expect_destroyed) {
    if (expect_destroyed) {
      EXPECT_EQ(fake_host_connection_factory_->GetActiveConnection(
                    remote_device.GetDeviceId()),
                nullptr);
    } else {
      EXPECT_NE(fake_host_connection_factory_->GetActiveConnection(
                    remote_device.GetDeviceId()),
                nullptr);
    }
  }

  void ConnectToWifi() {
    std::string wifi_service_path = helper_.ConfigureService(
        CreateConfigurationJsonString(kWifiNetworkGuid, shill::kTypeWifi));
  }

  std::vector<std::string> GetPreviouslyConnectedHostIds() {
    return previously_connected_host_ids_;
  }

  base::test::TaskEnvironment task_environment_;

  NetworkStateTestHelper helper_{/*use_default_devices_and_services=*/true};

  const multidevice::RemoteDeviceRefList test_remote_devices_;
  std::vector<std::string> test_remote_device_ids_;

  std::unique_ptr<FakeHostConnection::Factory> fake_host_connection_factory_;
  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<NiceMock<MockTetherHostResponseRecorder>>
      mock_tether_host_response_recorder_;
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> mock_timer_;

  std::unique_ptr<ConnectionPreserverImpl> connection_preserver_;

  std::vector<std::string> previously_connected_host_ids_;
};

TEST_F(ConnectionPreserverImplTest,
       TestHandleSuccessfulTetherAvailabilityResponse_NoPreservedConnection) {
  SimulateSuccessfulHostScan(test_remote_devices_[0],
                             true /* should_remain_registered */);
}

TEST_F(ConnectionPreserverImplTest,
       TestHandleSuccessfulTetherAvailabilityResponse_HasInternet) {
  ConnectToWifi();

  SimulateSuccessfulHostScan(test_remote_devices_[0],
                             false /* should_remain_registered */);
}

TEST_F(
    ConnectionPreserverImplTest,
    TestHandleSuccessfulTetherAvailabilityResponse_PreservedConnectionExists_NoPreviouslyConnectedHosts) {
  SimulateSuccessfulHostScan(test_remote_devices_[0],
                             true /* should_remain_registered */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[0],
                                        false /* expect_destroyed */);

  SimulateSuccessfulHostScan(test_remote_devices_[1],
                             true /* should_remain_registered */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[0],
                                        true /* expect_destroyed */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[1],
                                        false /* expect_destroyed */);
}

TEST_F(ConnectionPreserverImplTest,
       TestHandleSuccessfulTetherAvailabilityResponse_TimesOut) {
  SimulateSuccessfulHostScan(test_remote_devices_[0],
                             true /* should_remain_registered */);

  mock_timer_->Fire();
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[0],
                                        true /* expect_destroyed */);
}

TEST_F(ConnectionPreserverImplTest,
       TestHandleSuccessfulTetherAvailabilityResponse_PreserverDestroyed) {
  SimulateSuccessfulHostScan(test_remote_devices_[0],
                             true /* should_remain_registered */);

  connection_preserver_.reset();
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[0],
                                        true /* expect_destroyed */);
}

TEST_F(
    ConnectionPreserverImplTest,
    TestHandleSuccessfulTetherAvailabilityResponse_ActiveHostBecomesConnected) {
  SimulateSuccessfulHostScan(test_remote_devices_[0],
                             true /* should_remain_registered */);

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
  // |test_remote_device_ids_[0]| is the most recently connected device, and
  // should be preferred over any other device.
  previously_connected_host_ids_.push_back(test_remote_device_ids_[0]);
  previously_connected_host_ids_.push_back(test_remote_device_ids_[1]);

  SimulateSuccessfulHostScan(test_remote_devices_[2],
                             true /* should_remain_registered */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[2],
                                        false /* expect_destroyed */);

  SimulateSuccessfulHostScan(test_remote_devices_[1],
                             true /* should_remain_registered */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[2],
                                        true /* expect_destroyed */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[1],
                                        false /* expect_destroyed */);

  SimulateSuccessfulHostScan(test_remote_devices_[0],
                             true /* should_remain_registered */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[1],
                                        true /* expect_destroyed */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[0],
                                        false /* expect_destroyed */);

  SimulateSuccessfulHostScan(test_remote_devices_[1],
                             false /* should_remain_registered */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[0],
                                        false /* expect_destroyed */);

  SimulateSuccessfulHostScan(test_remote_devices_[2],
                             false /* should_remain_registered */);
  VerifyChannelForRemoteDeviceDestroyed(test_remote_devices_[0],
                                        false /* expect_destroyed */);
}

}  // namespace ash::tether
