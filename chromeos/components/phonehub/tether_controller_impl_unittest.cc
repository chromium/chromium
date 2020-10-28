// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/tether_controller_impl.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {
namespace {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;
using network_config::mojom::ConnectionStateType;
using network_config::mojom::NetworkStatePropertiesPtr;
using network_config::mojom::StartConnectResult;

constexpr char kWifiGuid[] = "WifiGuid";
constexpr char kTetherGuid[] = "TetherGuid";
constexpr char kTetherNetworkName[] = "TetherNetworkName";
constexpr char kTetherNetworkCarrier[] = "TetherNetworkCarrier";
constexpr int kBatteryPercentage = 100;
constexpr int kSignalStrength = 100;
constexpr bool kHasConnectedToHost = true;

class FakeObserver : public TetherController::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_status_changes() const { return num_status_changes_; }
  size_t num_scan_failed() const { return num_scan_failed_; }

  // TetherController::Observer:
  void OnTetherStatusChanged() override { ++num_status_changes_; }
  void OnAttemptConnectionScanFailed() override { ++num_scan_failed_; }

 private:
  size_t num_status_changes_ = 0;
  size_t num_scan_failed_ = 0;
};

}  // namespace

class TetherControllerImplTest : public testing::Test {
 protected:
  friend class TetherControllerImpl;

  TetherControllerImplTest() = default;
  TetherControllerImplTest(const TetherControllerImplTest&) = delete;
  TetherControllerImplTest& operator=(const TetherControllerImplTest&) = delete;
  ~TetherControllerImplTest() override = default;

  class FakeTetherNetworkConnector
      : public TetherControllerImpl::TetherNetworkConnector {
   public:
    FakeTetherNetworkConnector() = default;
    ~FakeTetherNetworkConnector() override = default;

    void StartConnect(const std::string& guid,
                      StartConnectCallback callback) override {
      start_connect_callback_ = std::move(callback);
    }

    void StartDisconnect(const std::string& guid,
                         StartDisconnectCallback callback) override {
      start_disconnect_callback_ = std::move(callback);
    }

    bool DoesPendingStartConnectCallbackExist() {
      return !start_connect_callback_.is_null();
    }

    bool DoesPendingDisconnectCallbackExist() {
      return !start_disconnect_callback_.is_null();
    }

    void InvokeStartConnectCallback(network_config::mojom::StartConnectResult
                                        result = StartConnectResult::kSuccess,
                                    const std::string& message = "") {
      std::move(start_connect_callback_).Run(result, message);
    }

    void InvokeDisconnectCallback(bool success = true) {
      std::move(start_disconnect_callback_).Run(success);
    }

   private:
    StartConnectCallback start_connect_callback_;
    StartDisconnectCallback start_disconnect_callback_;
  };

  // testing::Test:
  void SetUp() override {
    NetworkHandler::Initialize();
    base::RunLoop().RunUntilIdle();

    service_path_ =
        cros_network_config_helper_.network_state_helper().ConfigureService(
            base::StringPrintf(
                R"({"GUID": "%s", "Type": "wifi",
             "State": "ready", "Strength": 100,
            "Connectable": true})",
                kWifiGuid));

    controller_ =
        base::WrapUnique<TetherControllerImpl>(new TetherControllerImpl(
            &fake_multidevice_setup_client_,
            std::make_unique<FakeTetherNetworkConnector>()));
    controller_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    controller_->RemoveObserver(&fake_observer_);
    chromeos::NetworkHandler::Shutdown();
    testing::Test::TearDown();
  }

  NetworkStateHandler* network_state_handler() {
    return cros_network_config_helper_.network_state_helper()
        .network_state_handler();
  }

  FakeTetherNetworkConnector* fake_tether_network_connector() {
    return static_cast<FakeTetherNetworkConnector*>(
        controller_->connector_.get());
  }

  void EnableTetherDevice() {
    network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED);
    base::RunLoop().RunUntilIdle();
  }

  void DisconnectTetherDevice() {
    network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE);
    base::RunLoop().RunUntilIdle();
  }

  void AddVisibleTetherNetwork() {
    network_state_handler()->AddTetherNetworkState(
        kTetherGuid, kTetherNetworkName, kTetherNetworkCarrier,
        kBatteryPercentage, kSignalStrength, kHasConnectedToHost);
    network_state_handler()->AssociateTetherNetworkStateWithWifiNetwork(
        kTetherGuid, kWifiGuid);
    base::RunLoop().RunUntilIdle();
  }

  void RemoveVisibleTetherNetwork() {
    network_state_handler()->RemoveTetherNetworkState(kTetherGuid);
    base::RunLoop().RunUntilIdle();
  }

  void SetTetherNetworkStateConnected() {
    network_state_handler()->SetTetherNetworkStateConnected(kTetherGuid);
    base::RunLoop().RunUntilIdle();
  }

  void SetTetherNetworkStateConnecting() {
    network_state_handler()->SetTetherNetworkStateConnecting(kTetherGuid);
    base::RunLoop().RunUntilIdle();
  }

  void SetTetherNetworkStateDisconnected() {
    network_state_handler()->SetTetherNetworkStateDisconnected(kTetherGuid);
    base::RunLoop().RunUntilIdle();
  }

  void SetTetherScanState(bool is_scanning) {
    network_state_handler()->SetTetherScanState(is_scanning);
    base::RunLoop().RunUntilIdle();
  }

  TetherController::Status GetStatus() const {
    return controller_->GetStatus();
  }

  void SetMultideviceSetupFeatureState(FeatureState feature_state) {
    fake_multidevice_setup_client_.SetFeatureState(Feature::kInstantTethering,
                                                   feature_state);
  }

  void InvokePendingSetFeatureEnabledStateCallback(
      bool success,
      bool expected_enabled = true) {
    if (success)
      SetMultideviceSetupFeatureState(FeatureState::kEnabledByUser);

    fake_multidevice_setup_client_.InvokePendingSetFeatureEnabledStateCallback(
        Feature::kInstantTethering,
        /*expected_enabled=*/expected_enabled, base::nullopt, success);
  }

  void AttemptConnection() { controller_->AttemptConnection(); }

  void Disconnect() { controller_->Disconnect(); }

  size_t GetNumObserverStatusChanged() const {
    return fake_observer_.num_status_changes();
  }

  size_t GetNumObserverScanFailed() const {
    return fake_observer_.num_scan_failed();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_helper_;
  std::string service_path_;
  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;
  FakeObserver fake_observer_;
  std::unique_ptr<TetherControllerImpl> controller_;
};

TEST_F(TetherControllerImplTest, ExternalTetherChangesReflectToStatus) {
  EXPECT_EQ(GetStatus(), TetherController::Status::kIneligibleForFeature);
  SetMultideviceSetupFeatureState(FeatureState::kEnabledByUser);
  EXPECT_EQ(GetNumObserverStatusChanged(), 1U);
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionUnavailable);

  // Tether device and network must be enabled for status changes other than
  // kIneligibleForFeature or kConnectionUnavailable to occur.
  EnableTetherDevice();
  AddVisibleTetherNetwork();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionAvailable);
  EXPECT_EQ(GetNumObserverStatusChanged(), 2U);

  // Starts connecting to tether network.
  SetTetherNetworkStateConnecting();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);
  EXPECT_EQ(GetNumObserverStatusChanged(), 3U);

  // Connected to tether network.
  SetTetherNetworkStateConnected();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnected);
  EXPECT_EQ(GetNumObserverStatusChanged(), 4U);

  // Tether network disconnects on it's own.
  SetTetherNetworkStateDisconnected();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionAvailable);
  EXPECT_EQ(GetNumObserverStatusChanged(), 5U);

  // Tether network becomes unavailable.
  RemoveVisibleTetherNetwork();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionUnavailable);
  EXPECT_EQ(GetNumObserverStatusChanged(), 6U);
}

TEST_F(TetherControllerImplTest, AttemptConnectDisconnect) {
  SetMultideviceSetupFeatureState(FeatureState::kEnabledByUser);

  EnableTetherDevice();
  AddVisibleTetherNetwork();

  AttemptConnection();
  EXPECT_TRUE(
      fake_tether_network_connector()->DoesPendingStartConnectCallbackExist());

  // Upon completing the connection, the status should no longer be connecting.
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);
  fake_tether_network_connector()->InvokeStartConnectCallback();
  EXPECT_NE(GetStatus(), TetherController::Status::kConnecting);

  // Disconnect from a connected state.
  SetTetherNetworkStateConnected();
  Disconnect();
  EXPECT_TRUE(
      fake_tether_network_connector()->DoesPendingDisconnectCallbackExist());
  fake_tether_network_connector()->InvokeDisconnectCallback();
  SetTetherNetworkStateDisconnected();

  // Disconnect from a connecting state.
  AttemptConnection();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);
  Disconnect();
  EXPECT_TRUE(
      fake_tether_network_connector()->DoesPendingDisconnectCallbackExist());
  fake_tether_network_connector()->InvokeDisconnectCallback();

  AttemptConnection();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);
  Disconnect();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionAvailable);
  EXPECT_TRUE(
      fake_tether_network_connector()->DoesPendingDisconnectCallbackExist());
  AttemptConnection();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);
  fake_tether_network_connector()->InvokeDisconnectCallback();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);

  // Disconnect from a disconnected state.
  RemoveVisibleTetherNetwork();
  AttemptConnection();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);
  Disconnect();
  EXPECT_FALSE(
      fake_tether_network_connector()->DoesPendingDisconnectCallbackExist());
}

TEST_F(TetherControllerImplTest, AttemptConnectFeatureOffNetworkExists) {
  SetMultideviceSetupFeatureState(FeatureState::kDisabledByUser);
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionUnavailable);

  // Test enable flow when a tether device initially exists.
  EnableTetherDevice();
  AddVisibleTetherNetwork();
  AttemptConnection();

  // Should be set connecting even before feature is enabled.
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);

  // Should still be connecting when feature becomes enabled.
  InvokePendingSetFeatureEnabledStateCallback(/*success=*/true);
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);

  // Connecting to tether device.
  SetTetherNetworkStateConnecting();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);

  // Connected to tether network.
  AddVisibleTetherNetwork();
  SetTetherNetworkStateConnected();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnected);
  EXPECT_EQ(GetNumObserverStatusChanged(), 3U);

  // Tether network is lost.
  RemoveVisibleTetherNetwork();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionUnavailable);
}

TEST_F(TetherControllerImplTest, AttemptConnectFeatureFailedToEnable) {
  EnableTetherDevice();

  // Test enable flow when feature fails to turn on.
  SetMultideviceSetupFeatureState(FeatureState::kDisabledByUser);
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionUnavailable);
  AttemptConnection();

  // Should be set connecting even before feature is enabled.
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);

  // Should fail to connect if feature does not successfully turn on.
  InvokePendingSetFeatureEnabledStateCallback(/*success=*/false);
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionUnavailable);

  // Test when feature is enabled externally and visible network is added
  // before callback runs.
  AttemptConnection();

  // Should be set connecting even before feature is enabled.
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);

  // Feature enabled externally
  SetMultideviceSetupFeatureState(FeatureState::kEnabledByUser);
  AddVisibleTetherNetwork();

  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);

  // Should fail to connect if feature does not successfully turn on.
  InvokePendingSetFeatureEnabledStateCallback(/*success=*/false);
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionAvailable);

  RemoveVisibleTetherNetwork();

  // Test when Mulitdevice suite disabled before callback can return.
  AttemptConnection();

  // Should be set connecting even before feature is enabled.
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);

  // Disable suite externally.
  SetMultideviceSetupFeatureState(FeatureState::kDisabledByUser);
  AddVisibleTetherNetwork();

  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionUnavailable);
}

TEST_F(TetherControllerImplTest, AttemptConnectFeatureOffNoNetwork) {
  // Test enable flow when a tether device initially does not exist.
  DisconnectTetherDevice();
  SetMultideviceSetupFeatureState(FeatureState::kDisabledByUser);
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionUnavailable);
  AttemptConnection();

  // Should be set connecting even before feature is enabled.
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);

  // Should still be connecting when feature becomes enabled.
  InvokePendingSetFeatureEnabledStateCallback(/*success=*/true);
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);

  // Tether is scanning, connection should be connecting still.
  SetTetherScanState(true);
  EnableTetherDevice();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);
  DisconnectTetherDevice();

  // Tether stops scanning, attempt ends and connection should become
  // unavailable.
  SetTetherScanState(false);
  EnableTetherDevice();
  EXPECT_EQ(GetNumObserverScanFailed(), 1U);
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionUnavailable);

  // Tether starts scanning after connection attempt ended.
  SetTetherScanState(true);
  EnableTetherDevice();
  EXPECT_EQ(GetNumObserverScanFailed(), 1U);
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionUnavailable);
}

}  // namespace phonehub
}  // namespace chromeos
