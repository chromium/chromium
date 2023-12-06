// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/tether_controller_impl.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/phonehub/fake_user_action_recorder.h"
#include "chromeos/ash/components/phonehub/mutable_phone_model.h"
#include "chromeos/ash/components/phonehub/phone_model_test_util.h"
#include "chromeos/ash/components/phonehub/phone_status_model.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {

namespace {

using ::chromeos::network_config::mojom::ConnectionStateType;
using ::chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using ::chromeos::network_config::mojom::StartConnectResult;
using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;

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

PhoneStatusModel CreateTestPhoneStatusModel(
    PhoneStatusModel::MobileStatus mobile_status =
        PhoneStatusModel::MobileStatus::kSimWithReception) {
  PhoneStatusModel::MobileConnectionMetadata metadata;
  metadata.signal_strength = PhoneStatusModel::SignalStrength::kFourBars;
  metadata.mobile_provider =
      mobile_status == PhoneStatusModel::MobileStatus::kSimWithReception
          ? std::u16string(kFakeMobileProviderName)
          : std::u16string();
  return PhoneStatusModel(mobile_status, metadata,
                          PhoneStatusModel::ChargingState::kNotCharging,
                          PhoneStatusModel::BatterySaverState::kOff,
                          /*battery_percentage=*/100u);
}

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
    FakeTetherNetworkConnector() {
      network_config::BindToInProcessInstance(
          cros_network_config_.BindNewPipeAndPassReceiver());
    }
    ~FakeTetherNetworkConnector() override = default;

    void StartConnect(const std::string& guid,
                      StartConnectCallback callback) override {
      start_connect_callback_ = std::move(callback);
    }

    void StartDisconnect(const std::string& guid,
                         StartDisconnectCallback callback) override {
      start_disconnect_callback_ = std::move(callback);
    }

    void GetNetworkStateList(
        chromeos::network_config::mojom::NetworkFilterPtr filter,
        GetNetworkStateListCallback callback) override {
      cros_network_config_->GetNetworkStateList(
          std::move(filter),
          base::BindOnce(
              &FakeTetherNetworkConnector::OnVisibleTetherNetworkFetched,
              weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    }

    void SetNextConnectionStateType(ConnectionStateType connection_state) {
      connection_state_ = connection_state;
    }

    void OnVisibleTetherNetworkFetched(
        GetNetworkStateListCallback callback,
        std::vector<NetworkStatePropertiesPtr> networks) {
      if (connection_state_.has_value() && networks.size() == 1) {
        networks[0]->connection_state = *connection_state_;
        connection_state_ = std::nullopt;
      }

      std::move(callback).Run(std::move(networks));
    }

    bool DoesPendingStartConnectCallbackExist() {
      return !start_connect_callback_.is_null();
    }

    bool DoesPendingDisconnectCallbackExist() {
      return !start_disconnect_callback_.is_null();
    }

    void InvokeStartConnectCallbackWithFakeResult(
        StartConnectResult result = StartConnectResult::kSuccess,
        const std::string& message = "") {
      std::move(start_connect_callback_).Run(result, message);
    }

    void InvokeDisconnectCallbackWithRealResults(std::string service_path) {
      cros_network_config_->StartDisconnect(
          service_path, std::move(start_disconnect_callback_));
    }

    void InvokeDisconnectCallbackWithFakeParams(bool success = true) {
      std::move(start_disconnect_callback_).Run(success);
    }

   private:
    mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
        cros_network_config_;
    std::optional<ConnectionStateType> connection_state_;
    StartConnectCallback start_connect_callback_;
    StartDisconnectCallback start_disconnect_callback_;
    base::WeakPtrFactory<FakeTetherNetworkConnector> weak_ptr_factory_{this};
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

    fake_phone_model_.SetPhoneStatusModel(CreateTestPhoneStatusModel());

    controller_ =
        base::WrapUnique<TetherControllerImpl>(new TetherControllerImpl(
            &fake_phone_model_, &fake_user_action_recorder_,
            &fake_multidevice_setup_client_,
            std::make_unique<FakeTetherNetworkConnector>()));
    controller_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    controller_->RemoveObserver(&fake_observer_);
    NetworkHandler::Shutdown();
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

  void InvokeDisconnectCallbackWithRealResults() {
    fake_tether_network_connector()->InvokeDisconnectCallbackWithRealResults(
        service_path_);
    base::RunLoop().RunUntilIdle();
  }

  void InvokePendingSetFeatureEnabledStateCallback(
      bool success,
      bool expected_enabled = true) {
    if (success)
      SetMultideviceSetupFeatureState(FeatureState::kEnabledByUser);

    fake_multidevice_setup_client_.InvokePendingSetFeatureEnabledStateCallback(
        Feature::kInstantTethering,
        /*expected_enabled=*/expected_enabled, std::nullopt, success);
  }

  void AttemptConnection() {
    size_t num_recorded_connection_attempts_before_call =
        fake_user_action_recorder_.num_tether_attempts();
    controller_->AttemptConnection();
    EXPECT_EQ(num_recorded_connection_attempts_before_call + 1,
              fake_user_action_recorder_.num_tether_attempts());
  }

  void Disconnect() { controller_->Disconnect(); }

  size_t GetNumObserverStatusChanged() const {
    return fake_observer_.num_status_changes();
  }

  size_t GetNumObserverScanFailed() const {
    return fake_observer_.num_scan_failed();
  }

  MutablePhoneModel* phone_model() { return &fake_phone_model_; }

 private:
  base::test::TaskEnvironment task_environment_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_helper_;
  std::string service_path_;
  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;
  MutablePhoneModel fake_phone_model_;
  FakeUserActionRecorder fake_user_action_recorder_;
  FakeObserver fake_observer_;
  std::unique_ptr<TetherControllerImpl> controller_;
};

TEST_F(TetherControllerImplTest,
       DisconnectCompletesAfterOnActiveNetworksChanged) {
  SetMultideviceSetupFeatureState(FeatureState::kEnabledByUser);

  EnableTetherDevice();
  AddVisibleTetherNetwork();

  // Disconnect from a connecting state.
  AttemptConnection();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);

  SetTetherNetworkStateConnecting();
  Disconnect();

  // Simulate OnActiveNetworksChanged() being called after Disconnect()
  // is requested when Bluetooth is on but hotspot is off, yielding a
  // ConnectionStateType::kConnecting tether network instead of a
  // ConnectionStateType::kDisconnected network.
  fake_tether_network_connector()->SetNextConnectionStateType(
      ConnectionStateType::kConnecting);
  SetTetherNetworkStateDisconnected();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);

  // Upon invoking the Disconnect callback, a refetch occurs.
  EXPECT_TRUE(
      fake_tether_network_connector()->DoesPendingDisconnectCallbackExist());
  InvokeDisconnectCallbackWithRealResults();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionAvailable);
}

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

  // Phone status changed to no reception.
  phone_model()->SetPhoneStatusModel(CreateTestPhoneStatusModel(
      PhoneStatusModel::MobileStatus::kSimButNoReception));
  EXPECT_EQ(GetStatus(), TetherController::Status::kNoReception);
  EXPECT_EQ(GetNumObserverStatusChanged(), 7U);

  // Phone status changed to no SIM.
  phone_model()->SetPhoneStatusModel(
      CreateTestPhoneStatusModel(PhoneStatusModel::MobileStatus::kNoSim));
  EXPECT_EQ(GetStatus(), TetherController::Status::kNoReception);
  EXPECT_EQ(GetNumObserverStatusChanged(), 7U);

  // Tether feature becomes unsupported,
  SetMultideviceSetupFeatureState(FeatureState::kNotSupportedByPhone);
  EXPECT_EQ(GetStatus(), TetherController::Status::kIneligibleForFeature);
  EXPECT_EQ(GetNumObserverStatusChanged(), 8U);

  // Tether feature becomes supported, the status becomes kNoReception again.
  SetMultideviceSetupFeatureState(FeatureState::kEnabledByUser);
  EXPECT_EQ(GetStatus(), TetherController::Status::kNoReception);
  EXPECT_EQ(GetNumObserverStatusChanged(), 9U);

  // Phone status changed to having reception. Connection is still unavailable.
  phone_model()->SetPhoneStatusModel(CreateTestPhoneStatusModel(
      PhoneStatusModel::MobileStatus::kSimWithReception));
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionUnavailable);
  EXPECT_EQ(GetNumObserverStatusChanged(), 10U);

  // Phone Model is lost, connection is still unavailable.
  phone_model()->SetPhoneStatusModel(std::nullopt);
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionUnavailable);
  EXPECT_EQ(GetNumObserverStatusChanged(), 10U);

  // Even though there is no Phone Model, adding a visible tether network
  // will cause the controller to indicate a connection is available.
  AddVisibleTetherNetwork();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionAvailable);
  EXPECT_EQ(GetNumObserverStatusChanged(), 11U);

  // Even though there is no Phone Model, connecting to a visible tether network
  // externally (e.g via OS Settings) will cause the controller to indicate a
  // connecting tether state.
  SetTetherNetworkStateConnecting();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);
  EXPECT_EQ(GetNumObserverStatusChanged(), 12U);

  // Even though there is no Phone Model, a connection to a visible tether
  // network externally (e.g via OS Settings) will cause the controller to
  // indicate a connected tether state.
  SetTetherNetworkStateConnected();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnected);
  EXPECT_EQ(GetNumObserverStatusChanged(), 13U);
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
  fake_tether_network_connector()->InvokeStartConnectCallbackWithFakeResult();
  EXPECT_NE(GetStatus(), TetherController::Status::kConnecting);

  // Disconnect from a connected state.
  SetTetherNetworkStateConnected();
  Disconnect();
  EXPECT_TRUE(
      fake_tether_network_connector()->DoesPendingDisconnectCallbackExist());
  fake_tether_network_connector()->InvokeDisconnectCallbackWithFakeParams();
  SetTetherNetworkStateDisconnected();

  // Disconnect from a connecting state.
  AttemptConnection();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);
  Disconnect();
  EXPECT_TRUE(
      fake_tether_network_connector()->DoesPendingDisconnectCallbackExist());
  fake_tether_network_connector()->InvokeDisconnectCallbackWithFakeParams();

  AttemptConnection();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);
  Disconnect();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionAvailable);
  EXPECT_TRUE(
      fake_tether_network_connector()->DoesPendingDisconnectCallbackExist());
  AttemptConnection();
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);
  fake_tether_network_connector()->InvokeDisconnectCallbackWithFakeParams();
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
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnecting);
  DisconnectTetherDevice();

  // Tether stops scanning, attempt ends and connection should become
  // unavailable.
  SetTetherScanState(false);
  EXPECT_EQ(GetNumObserverScanFailed(), 1U);
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionUnavailable);

  // Tether starts scanning after connection attempt ended.
  SetTetherScanState(true);
  EXPECT_EQ(GetNumObserverScanFailed(), 1U);
  EXPECT_EQ(GetStatus(), TetherController::Status::kConnectionUnavailable);
}

}  // namespace phonehub
}  // namespace ash
