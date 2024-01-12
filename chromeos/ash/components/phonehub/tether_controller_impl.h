// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_TETHER_CONTROLLER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_TETHER_CONTROLLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/phonehub/phone_model.h"
#include "chromeos/ash/components/phonehub/tether_controller.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace phonehub {

class UserActionRecorder;

// TetherController implementation which utilizes MultiDeviceSetupClient and
// CrosNetworkConfig in order to interact with Instant Tethering. If Instant
// Tethering is user disabled, AttemptConnection() will first enable the feature
// via the MultiDeviceSetupClient, then scan for an eligible phone via
// CrosNetworkConfig, and finally connect to the phone via CrosNetworkConfig. If
// Instant Tethering is enabled, but there is no visible Tether network,
// AttemptConnection() will first scan for an eligible phone via
// CrosNetworkConfig, and connect to the phone via CrosNetworkConfig. If Instant
// Tethering is enabled and there is a visible Tether Network previously fetched
// from observing CrosNetworkConfig, AttemptConnection() will just connect to
// the phone via CrosNetworkConfig. Disconnect() disconnects the Tether network
// if one exists.
class TetherControllerImpl
    : public TetherController,
      public PhoneModel::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer,
      public chromeos::network_config::CrosNetworkConfigObserver {
 public:
  TetherControllerImpl(
      PhoneModel* phone_model,
      UserActionRecorder* user_action_recorder,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);
  ~TetherControllerImpl() override;

  // TetherController:
  Status GetStatus() const override;
  void ScanForAvailableConnection() override;
  void AttemptConnection() override;
  void Disconnect() override;

 private:
  friend class TetherControllerImplTest;

  // Used to track AttemptConnection() and Disconnect() calls.
  enum class ConnectDisconnectStatus {
    // No AttemptConnection or Disconnect is in progress. The class still
    // observes changes in the Tether network initiated externally (e.g in OS
    // Settings), and causes changes to the |status_|.
    kIdle = 0,

    // Used in AttemptConnection flow. Enabling the InstantTethering feature as
    // it was previously disabled.
    kTurningOnInstantTethering = 1,

    // Used in AttemptConnection flow. Requesting a scan has has no callback, so
    // this state is changed upon observing tether network changes or device
    // changes. If a visible Tether network is observed, the
    // |connect_disconnect_status_| will change to kConnectingToEligiblePhone.
    // If a visible Tether network is not observed by the time the Tether device
    // stops scanning, the |connect_disconnect_status_| will change back to
    // kIdle.
    // Note: Calling ScanForAvailableConnection() will not set the
    // |connect_disconnect_status_| to this value.
    kScanningForEligiblePhone = 2,

    // Used in AttemptConnection flow. In the process of connecting to a Tether
    // Network.
    kConnectingToEligiblePhone = 3,

    // Used in Disconnect flow. Disconnects from the tether network.
    kDisconnecting = 4,
  };

  // Connector that uses CrosNetworkConfig to connect, disconnect, and get the
  // network state list. This class is used for testing purposes.
  class TetherNetworkConnector {
   public:
    using StartConnectCallback = base::OnceCallback<void(
        chromeos::network_config::mojom::StartConnectResult result,
        const std::string& message)>;

    using StartDisconnectCallback = base::OnceCallback<void(bool)>;

    using GetNetworkStateListCallback = base::OnceCallback<void(
        std::vector<
            chromeos::network_config::mojom::NetworkStatePropertiesPtr>)>;

    TetherNetworkConnector();
    TetherNetworkConnector(const TetherNetworkConnector&) = delete;
    TetherNetworkConnector& operator=(const TetherNetworkConnector&) = delete;
    virtual ~TetherNetworkConnector();

    virtual void StartConnect(const std::string& guid,
                              StartConnectCallback callback);
    virtual void StartDisconnect(const std::string& guid,
                                 StartDisconnectCallback callback);
    virtual void GetNetworkStateList(
        chromeos::network_config::mojom::NetworkFilterPtr filter,
        GetNetworkStateListCallback callback);

   private:
    mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
        cros_network_config_;
  };

  // Two parameter constructor made available for testing purposes. The one
  // parameter constructor calls this constructor.
  TetherControllerImpl(
      PhoneModel* phone_model,
      UserActionRecorder* user_action_recorder,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      std::unique_ptr<TetherControllerImpl::TetherNetworkConnector> connector);

  // PhoneModel::Observer:
  void OnModelChanged() override;

  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  // CrosNetworkConfigObserver:
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks) override;
  void OnNetworkStateListChanged() override;
  void OnDeviceStateListChanged() override;

  void AttemptTurningOnTethering();
  void OnSetFeatureEnabled(bool success);
  void PerformConnectionAttempt();
  void StartConnect();
  void OnStartConnectCompleted(
      chromeos::network_config::mojom::StartConnectResult result,
      const std::string& message);
  void OnDisconnectCompleted(bool success);
  void FetchVisibleTetherNetwork();
  void OnGetDeviceStateList(
      std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
          devices);
  void OnVisibleTetherNetworkFetched(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);
  void SetConnectDisconnectStatus(
      ConnectDisconnectStatus connect_disconnect_status);
  void UpdateStatus();
  TetherController::Status ComputeStatus() const;

  raw_ptr<PhoneModel> phone_model_;
  raw_ptr<UserActionRecorder> user_action_recorder_;
  raw_ptr<multidevice_setup::MultiDeviceSetupClient> multidevice_setup_client_;
  ConnectDisconnectStatus connect_disconnect_status_ =
      ConnectDisconnectStatus::kIdle;
  Status status_ = Status::kIneligibleForFeature;

  // Whether this class is attempting a tether connection.
  bool is_attempting_connection_ = false;

  chromeos::network_config::mojom::NetworkStatePropertiesPtr tether_network_;

  std::unique_ptr<TetherNetworkConnector> connector_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      receiver_{this};
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config_;

  base::WeakPtrFactory<TetherControllerImpl> weak_ptr_factory_{this};
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_TETHER_CONTROLLER_IMPL_H_
