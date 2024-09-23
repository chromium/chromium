// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_connect.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;
using testing::AnyNumber;
using testing::NiceMock;
using testing::Return;

namespace ash {

namespace {

const char kWiFi1ServicePath[] = "/service/wifi1";
const char kWiFi1Guid[] = "wifi1_guid";

const char kWiFiUnconfiguredServicePath[] = "/service/wifi_unconfigured";
const char kWiFiUnconfiguredGuid[] = "wifi_unconfigured_guid";

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
  MOCK_METHOD1(ShowCarrierAccountDetail, void(const std::string& network_id));
  MOCK_METHOD2(ShowPortalSignin,
               void(const std::string& network_id,
                    NetworkConnect::Source source));
  MOCK_METHOD2(ShowNetworkConnectError,
               void(const std::string& error_name,
                    const std::string& network_id));
  MOCK_METHOD1(ShowMobileActivationError, void(const std::string& network_id));
  MOCK_METHOD0(ShowCarrierUnlockNotification, void());
};

class FakeTetherDelegate : public NetworkConnectionHandler::TetherDelegate {
 public:
  FakeTetherDelegate() = default;
  ~FakeTetherDelegate() override = default;

  std::string last_connected_tether_network_guid() {
    return last_connected_tether_network_guid_;
  }

  // NetworkConnectionHandler::TetherDelegate:
  void ConnectToNetwork(const std::string& tether_network_guid,
                        base::OnceClosure success_callback,
                        StringErrorCallback error_callback) override {
    if (should_connect_to_network_id_succeed_) {
      last_connected_tether_network_guid_ = tether_network_guid;
      std::move(success_callback).Run();
    } else {
      std::move(error_callback)
          .Run(NetworkConnectionHandler::kErrorConnectFailed);
    }
  }
  void DisconnectFromNetwork(const std::string& tether_network_guid,
                             base::OnceClosure success_callback,
                             StringErrorCallback error_callback) override {}

  void ShouldConnectToNetworkIdSucceed(bool success) {
    should_connect_to_network_id_succeed_ = success;
  }

 private:
  std::string last_connected_tether_network_guid_;
  bool should_connect_to_network_id_succeed_ = true;
};

}  // namespace

class NetworkConnectTest : public testing::Test {
 public:
  NetworkConnectTest() = default;

  NetworkConnectTest(const NetworkConnectTest&) = delete;
  NetworkConnectTest& operator=(const NetworkConnectTest&) = delete;

  ~NetworkConnectTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    LoginState::Initialize();
    SetupDefaultShillState();
    base::RunLoop().RunUntilIdle();

    mock_delegate_ = std::make_unique<MockDelegate>();
    ON_CALL(*mock_delegate_, ShowEnrollNetwork(_)).WillByDefault(Return(true));

    fake_tether_delegate_ = std::make_unique<FakeTetherDelegate>();
    NetworkHandler::Get()->network_connection_handler()->SetTetherDelegate(
        fake_tether_delegate_.get());

    NetworkConnect::Initialize(mock_delegate_.get());
  }

  void TearDown() override {
    NetworkConnect::Shutdown();
    mock_delegate_.reset();
    LoginState::Shutdown();
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
                                      base::Value(shill::kSecurityClassWep));
    service_test_->SetServiceProperty(
        kWiFi1ServicePath, shill::kConnectableProperty, base::Value(true));
    service_test_->SetServiceProperty(
        kWiFi1ServicePath, shill::kPassphraseProperty, base::Value("password"));

    // Create an unconfigured wifi network.
    service_test_->AddService(kWiFiUnconfiguredServicePath,
                              kWiFiUnconfiguredGuid, "wifi_unconfigured",
                              shill::kTypeWifi, shill::kStateIdle,
                              add_to_visible);
    service_test_->SetServiceProperty(kWiFiUnconfiguredServicePath,
                                      shill::kSecurityClassProperty,
                                      base::Value(shill::kSecurityClassWep));
    service_test_->SetServiceProperty(kWiFiUnconfiguredServicePath,
                                      shill::kConnectableProperty,
                                      base::Value(false));
    service_test_->SetServiceProperty(kWiFiUnconfiguredServicePath,
                                      shill::kErrorProperty,
                                      base::Value("bad-password"));

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
  NetworkHandlerTestHelper network_handler_test_helper_;
  raw_ptr<ShillDeviceClient::TestInterface> device_test_;
  raw_ptr<ShillServiceClient::TestInterface> service_test_;
};

TEST_F(NetworkConnectTest, ConnectToNetworkId_NoConfiguration) {
  EXPECT_CALL(*mock_delegate_, ShowNetworkConfigure(_)).Times(0);
  EXPECT_CALL(*mock_delegate_, ShowNetworkConnectError(_, _)).Times(0);

  NetworkConnect::Get()->ConnectToNetworkId("bad guid");
}

TEST_F(NetworkConnectTest, ConnectToNetworkId_Unconfigured) {
  EXPECT_CALL(*mock_delegate_, ShowNetworkConfigure(_)).Times(1);
  EXPECT_CALL(*mock_delegate_, ShowNetworkConnectError(_, _)).Times(0);

  NetworkConnect::Get()->ConnectToNetworkId(kWiFiUnconfiguredGuid);
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkConnectTest, ConfigureAndConnectToNetwork_NoConfiguration) {
  EXPECT_CALL(*mock_delegate_,
              ShowNetworkConnectError(NetworkConnectionHandler::kErrorNotFound,
                                      "bad guid"));

  base::Value::Dict properties;
  NetworkConnect::Get()->ConfigureNetworkIdAndConnect("bad guid", properties,
                                                      true);
}

TEST_F(NetworkConnectTest,
       ConfigureAndConnectToNetwork_NotSharedButNoProfilePath) {
  EXPECT_CALL(*mock_delegate_,
              ShowNetworkConnectError(
                  NetworkConnectionHandler::kErrorConfigureFailed, kWiFi1Guid));

  base::Value::Dict properties;
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

TEST_F(NetworkConnectTest, ConnectToTetherNetwork_ConnectError) {
  EXPECT_CALL(*mock_delegate_, ShowNetworkConfigure(_)).Times(0);

  AddTetherNetwork(/*has_connected_to_host=*/true);
  fake_tether_delegate_->ShouldConnectToNetworkIdSucceed(/*success=*/false);
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

TEST_F(NetworkConnectTest, CarrierUnlock) {
  EXPECT_CALL(*mock_delegate_, ShowCarrierUnlockNotification());
  NetworkConnect::Get()->ShowCarrierUnlockNotification();
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

TEST_F(NetworkConnectTest, ConnectToCellularNetwork_OutOfCredits) {
  EXPECT_CALL(*mock_delegate_, ShowCarrierAccountDetail(kCellular1Guid));

  service_test_->SetServiceProperty(
      kCellular1ServicePath, shill::kConnectableProperty, base::Value(false));
  service_test_->SetServiceProperty(
      kCellular1ServicePath, shill::kOutOfCreditsProperty, base::Value(true));
  base::RunLoop().RunUntilIdle();

  NetworkConnect::Get()->ConnectToNetworkId(kCellular1Guid);
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkConnectTest, ShowPortalSignin) {
  EXPECT_CALL(*mock_delegate_, ShowPortalSignin(kWiFi1Guid, _));

  service_test_->SetServiceProperty(kWiFi1ServicePath, shill::kStateProperty,
                                    base::Value(shill::kStateRedirectFound));
  base::RunLoop().RunUntilIdle();

  NetworkConnect::Get()->ShowPortalSignin(kWiFi1Guid,
                                          NetworkConnect::Source::kSettings);
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkConnectTest, ConnectToCellularNetwork_SimLocked) {
  EXPECT_CALL(*mock_delegate_, ShowNetworkSettings(kCellular1Guid)).Times(0);

  service_test_->SetServiceProperty(kCellular1ServicePath,
                                    shill::kErrorProperty,
                                    base::Value(shill::kErrorSimLocked));
  service_test_->SetErrorForNextConnectionAttempt(shill::kErrorConnectFailed);

  NetworkConnect::Get()->ConnectToNetworkId(kCellular1Guid);
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkConnectTest, ConnectToCellularNetwork_SimCarrierLocked) {
  EXPECT_CALL(*mock_delegate_, ShowNetworkSettings(kCellular1Guid)).Times(0);

  service_test_->SetServiceProperty(kCellular1ServicePath,
                                    shill::kErrorProperty,
                                    base::Value(shill::kErrorSimCarrierLocked));
  service_test_->SetErrorForNextConnectionAttempt(shill::kErrorConnectFailed);

  NetworkConnect::Get()->ConnectToNetworkId(kCellular1Guid);
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash
