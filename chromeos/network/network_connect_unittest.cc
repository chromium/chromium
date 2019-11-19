// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_connect.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;
using testing::AnyNumber;
using testing::NiceMock;
using testing::Return;

namespace chromeos {

namespace {

const char kWiFi1ServicePath[] = "/service/wifi1";
const char kWiFi1Guid[] = "wifi1_guid";

const char kCellular1DevicePath[] = "/device/stub_cellular_device1";
const char kCellular1ServicePath[] = "/service/cellular1";
const char kCellular1Guid[] = "cellular1_guid";

const char kTetherGuid[] = "tether_guid";

class MockDelegate : public NetworkConnect::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD1(ShowNetworkConfigure, void(const std::string& network_id));
  MOCK_METHOD1(ShowNetworkSettings, void(const std::string& network_id));
  MOCK_METHOD1(ShowEnrollNetwork, bool(const std::string& network_id));
  MOCK_METHOD1(ShowMobileSetupDialog, void(const std::string& network_id));
  MOCK_METHOD2(ShowNetworkConnectError,
               void(const std::string& error_name,
                    const std::string& network_id));
  MOCK_METHOD1(ShowMobileActivationError, void(const std::string& network_id));
};

class FakeTetherDelegate : public NetworkConnectionHandler::TetherDelegate {
 public:
  FakeTetherDelegate() = default;
  ~FakeTetherDelegate() override = default;

  std::string last_connected_tether_network_guid() {
    return last_connected_tether_network_guid_;
  }

  // NetworkConnectionHandler::TetherDelegate:
  void ConnectToNetwork(
      const std::string& tether_network_guid,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback) override {
    last_connected_tether_network_guid_ = tether_network_guid;
    success_callback.Run();
  }
  void DisconnectFromNetwork(
      const std::string& tether_network_guid,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback) override {}

 private:
  std::string last_connected_tether_network_guid_;
};

}  // namespace

class NetworkConnectTest : public testing::Test {
 public:
  NetworkConnectTest() = default;
  ~NetworkConnectTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    shill_clients::InitializeFakes();
    LoginState::Initialize();
    SetupDefaultShillState();
    NetworkHandler::Initialize();
    base::RunLoop().RunUntilIdle();

    mock_delegate_.reset(new MockDelegate());
    ON_CALL(*mock_delegate_, ShowEnrollNetwork(_)).WillByDefault(Return(true));

    fake_tether_delegate_.reset(new FakeTetherDelegate());
    NetworkHandler::Get()->network_connection_handler()->SetTetherDelegate(
        fake_tether_delegate_.get());

    NetworkConnect::Initialize(mock_delegate_.get());
  }

  void TearDown() override {
    NetworkConnect::Shutdown();
    mock_delegate_.reset();
    LoginState::Shutdown();
    NetworkHandler::Shutdown();
    shill_clients::Shutdown();
    testing::Test::TearDown();
  }

 protected:
  void SetupDefaultShillState() {
    base::RunLoop().RunUntilIdle();
    device_test_ = ShillDeviceClient::Get()->GetTestInterface();
    device_test_->ClearDevices();
    device_test_->AddDevice("/device/stub_wifi_device1", shill::kTypeWifi,
                            "stub_wifi_device1");
    device_test_->AddDevice(kCellular1DevicePath, shill::kTypeCellular,
                            "stub_cellular_device1");
    device_test_->SetDeviceProperty(
        kCellular1DevicePath, shill::kTechnologyFamilyProperty,
        base::Value(shill::kNetworkTechnologyGsm), /*notify_changed=*/true);

    service_test_ = ShillServiceClient::Get()->GetTestInterface();
    service_test_->ClearServices();
    const bool add_to_visible = true;

    // Create a wifi network and set to online.
    service_test_->AddService(kWiFi1ServicePath, kWiFi1Guid, "wifi1",
                              shill::kTypeWifi, shill::kStateIdle,
                              add_to_visible);
    service_test_->SetServiceProperty(kWiFi1ServicePath,
                                      shill::kSecurityClassProperty,
                                      base::Value(shill::kSecurityWep));
    service_test_->SetServiceProperty(
        kWiFi1ServicePath, shill::kConnectableProperty, base::Value(true));
    service_test_->SetServiceProperty(
        kWiFi1ServicePath, shill::kPassphraseProperty, base::Value("password"));

    // Create a cellular network.
    service_test_->AddService(kCellular1ServicePath, kCellular1Guid,
                              "cellular1", shill::kTypeCellular,
                              shill::kStateIdle, add_to_visible);
    service_test_->SetServiceProperty(
        kCellular1ServicePath, shill::kConnectableProperty, base::Value(true));
    service_test_->SetServiceProperty(
        kCellular1ServicePath, shill::kActivationStateProperty,
        base::Value(shill::kActivationStateActivated));
    service_test_->SetServiceProperty(kCellular1ServicePath,
                                      shill::kOutOfCreditsProperty,
                                      base::Value(false));

    base::RunLoop().RunUntilIdle();
  }

  void AddTetherNetwork(bool has_connected_to_host) {
    NetworkStateHandler* handler =
        NetworkHandler::Get()->network_state_handler();
    handler->SetTetherTechnologyState(
        NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED);
    handler->AddTetherNetworkState(kTetherGuid, "TetherName", "TetherCarrier",
                                   100 /* battery_percentage */,
                                   100 /* signal_strength */,
                                   has_connected_to_host);
  }

  std::unique_ptr<MockDelegate> mock_delegate_;
  std::unique_ptr<FakeTetherDelegate> fake_tether_delegate_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  ShillDeviceClient::TestInterface* device_test_;
  ShillServiceClient::TestInterface* service_test_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkConnectTest);
};

TEST_F(NetworkConnectTest, ConnectToNetworkId_NoConfiguration) {
  EXPECT_CALL(*mock_delegate_, ShowNetworkConfigure(_)).Times(0);
  EXPECT_CALL(*mock_delegate_, ShowNetworkConnectError(_, _)).Times(0);

  NetworkConnect::Get()->ConnectToNetworkId("bad guid");
}

TEST_F(NetworkConnectTest, ConfigureAndConnectToNetwork_NoConfiguration) {
  EXPECT_CALL(*mock_delegate_,
              ShowNetworkConnectError(NetworkConnectionHandler::kErrorNotFound,
                                      "bad guid"));

  base::DictionaryValue properties;
  NetworkConnect::Get()->ConfigureNetworkIdAndConnect("bad guid", properties,
                                                      true);
}

TEST_F(NetworkConnectTest,
       ConfigureAndConnectToNetwork_NotSharedButNoProfilePath) {
  EXPECT_CALL(*mock_delegate_,
              ShowNetworkConnectError(
                  NetworkConnectionHandler::kErrorConfigureFailed, kWiFi1Guid));

  base::DictionaryValue properties;
  NetworkConnect::Get()->ConfigureNetworkIdAndConnect(kWiFi1Guid, properties,
                                                      false);
}

TEST_F(NetworkConnectTest, ConnectThenDisconnectWiFiNetwork) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          kWiFi1Guid);

  NetworkConnect::Get()->ConnectToNetworkId(kWiFi1Guid);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(network->IsConnectedState());

  NetworkConnect::Get()->DisconnectFromNetworkId(kWiFi1Guid);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(network->IsConnectedState());
  EXPECT_FALSE(network->IsConnectingState());
}

TEST_F(NetworkConnectTest, ConnectToTetherNetwork_HasConnectedToHost) {
  EXPECT_CALL(*mock_delegate_, ShowNetworkConfigure(_)).Times(0);

  AddTetherNetwork(true /* has_connected_to_host */);

  NetworkConnect::Get()->ConnectToNetworkId(kTetherGuid);
  EXPECT_EQ(kTetherGuid,
            fake_tether_delegate_->last_connected_tether_network_guid());
}

TEST_F(NetworkConnectTest, ConnectToTetherNetwork_HasNotConnectedToHost) {
  EXPECT_CALL(*mock_delegate_, ShowNetworkConfigure(_));

  AddTetherNetwork(false /* has_connected_to_host */);

  NetworkConnect::Get()->ConnectToNetworkId(kTetherGuid);
  EXPECT_TRUE(
      fake_tether_delegate_->last_connected_tether_network_guid().empty());
}

TEST_F(NetworkConnectTest, ActivateCellular) {
  EXPECT_CALL(*mock_delegate_, ShowMobileSetupDialog(kCellular1Guid));

  service_test_->SetServiceProperty(
      kCellular1ServicePath, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateNotActivated));
  base::RunLoop().RunUntilIdle();

  NetworkConnect::Get()->ConnectToNetworkId(kCellular1Guid);
}

TEST_F(NetworkConnectTest, ActivateCellular_Error) {
  EXPECT_CALL(*mock_delegate_, ShowMobileActivationError(kCellular1Guid));

  service_test_->SetServiceProperty(
      kCellular1ServicePath, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateNotActivated));
  service_test_->SetServiceProperty(
      kCellular1ServicePath, shill::kActivationTypeProperty,
      base::Value(shill::kActivationTypeNonCellular));
  base::RunLoop().RunUntilIdle();

  NetworkConnect::Get()->ConnectToNetworkId(kCellular1Guid);
}

}  // namespace chromeos
