// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/active_host_network_state_updater.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "chromeos/components/tether/fake_active_host.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace tether {

namespace {

const char kFakeDeviceId[] = "fakeDeviceId";
const char kWifiNetworkGuid[] = "wifiNetworkGuid";
const char kTetherNetworkGuid[] = "tetherNetworkGuid";

std::string CreateWifiConfigurationJsonString(const std::string& guid) {
  std::stringstream ss;
  ss << "{"
     << "  \"GUID\": \"" << guid << "\","
     << "  \"Type\": \"" << shill::kTypeWifi << "\","
     << "  \"State\": \"" << shill::kStateIdle << "\""
     << "}";
  return ss.str();
}

}  // namespace

class ActiveHostNetworkStateUpdaterTest : public testing::Test {
 protected:
  ActiveHostNetworkStateUpdaterTest() {}
  ~ActiveHostNetworkStateUpdaterTest() override = default;

  void SetUp() override {
    helper_.network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TECHNOLOGY_ENABLED);
    SetUpTetherNetwork();
    SetUpWifiNetwork();

    fake_active_host_ = std::make_unique<FakeActiveHost>();

    updater_ = base::WrapUnique(new ActiveHostNetworkStateUpdater(
        fake_active_host_.get(), helper_.network_state_handler()));
  }

  void SetUpTetherNetwork() {
    // Add tether network whose status will be changed during the test.
    helper_.network_state_handler()->AddTetherNetworkState(
        kTetherNetworkGuid, "TetherNetworkName", "TetherNetworkCarrier",
        100 /* battery_percentage */, 100 /* signal_strength */,
        true /* has_connected_to_host */);
  }

  void SetUpWifiNetwork() {
    helper_.ConfigureService(
        CreateWifiConfigurationJsonString(kWifiNetworkGuid));
    helper_.network_state_handler()->AssociateTetherNetworkStateWithWifiNetwork(
        kTetherNetworkGuid, kWifiNetworkGuid);
  }

  void TearDown() override {}

  void VerifyDisconnected() {
    const NetworkState* network_state =
        helper_.network_state_handler()->GetNetworkStateFromGuid(
            kTetherNetworkGuid);
    EXPECT_TRUE(network_state);
    EXPECT_TRUE(!network_state->IsConnectedState() &&
                !network_state->IsConnectingState());
  }

  void VerifyConnecting() {
    const NetworkState* network_state =
        helper_.network_state_handler()->GetNetworkStateFromGuid(
            kTetherNetworkGuid);
    EXPECT_TRUE(network_state);
    EXPECT_TRUE(network_state->IsConnectingState());
  }

  void VerifyConnected() {
    const NetworkState* network_state =
        helper_.network_state_handler()->GetNetworkStateFromGuid(
            kTetherNetworkGuid);
    EXPECT_TRUE(network_state);
    EXPECT_TRUE(network_state->IsConnectedState());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  NetworkStateTestHelper helper_{true /* use_default_devices_and_services */};

  std::unique_ptr<FakeActiveHost> fake_active_host_;

  std::unique_ptr<ActiveHostNetworkStateUpdater> updater_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ActiveHostNetworkStateUpdaterTest);
};

TEST_F(ActiveHostNetworkStateUpdaterTest, TestActiveHostUpdates) {
  VerifyDisconnected();

  fake_active_host_->SetActiveHostConnecting(kFakeDeviceId, kTetherNetworkGuid);
  VerifyConnecting();

  fake_active_host_->SetActiveHostConnected(kFakeDeviceId, kTetherNetworkGuid,
                                            kWifiNetworkGuid);
  VerifyConnected();

  fake_active_host_->SetActiveHostDisconnected();
  VerifyDisconnected();
}

}  // namespace tether

}  // namespace chromeos
