// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/tether_controller_impl.h"

#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/phone_status_model.h"
#include "chromeos/ash/components/phonehub/user_action_recorder.h"
#include "chromeos/ash/components/phonehub/util/histogram_util.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"

namespace ash {
namespace phonehub {

namespace {

using ::chromeos::network_config::mojom::ConnectionStateType;
using ::chromeos::network_config::mojom::DeviceStatePropertiesPtr;
using ::chromeos::network_config::mojom::FilterType;
using ::chromeos::network_config::mojom::NetworkType;
using ::chromeos::network_config::mojom::StartConnectResult;
using multidevice_setup::MultiDeviceSetupClient;
using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;

}  // namespace

TetherControllerImpl::TetherNetworkConnector::TetherNetworkConnector() {
  network_config::BindToInProcessInstance(
      cros_network_config_.BindNewPipeAndPassReceiver());
}

TetherControllerImpl::TetherNetworkConnector::~TetherNetworkConnector() =
    default;

void TetherControllerImpl::TetherNetworkConnector::StartConnect(
    const std::string& guid,
    StartConnectCallback callback) {
  cros_network_config_->StartConnect(guid, std::move(callback));
}

void TetherControllerImpl::TetherNetworkConnector::StartDisconnect(
    const std::string& guid,
    StartDisconnectCallback callback) {
  cros_network_config_->StartDisconnect(guid, std::move(callback));
}

void TetherControllerImpl::TetherNetworkConnector::GetNetworkStateList(
    chromeos::network_config::mojom::NetworkFilterPtr filter,
    GetNetworkStateListCallback callback) {
  cros_network_config_->GetNetworkStateList(std::move(filter),
                                            std::move(callback));
}

TetherControllerImpl::TetherControllerImpl(
    PhoneModel* phone_model,
    UserActionRecorder* user_action_recorder,
    MultiDeviceSetupClient* multidevice_setup_client)
    : TetherControllerImpl(
          phone_model,
          user_action_recorder,
          multidevice_setup_client,
          std::make_unique<TetherControllerImpl::TetherNetworkConnector>()) {}

TetherControllerImpl::TetherControllerImpl(
    PhoneModel* phone_model,
    UserActionRecorder* user_action_recorder,
    MultiDeviceSetupClient* multidevice_setup_client,
    std::unique_ptr<TetherControllerImpl::TetherNetworkConnector> connector)
    : phone_model_(phone_model),
      user_action_recorder_(user_action_recorder),
      multidevice_setup_client_(multidevice_setup_client),
      connector_(std::move(connector)) {
  // Receive updates when devices (e.g., Tether, Ethernet, Wi-Fi) go on/offline
  // This class only cares about Tether devices.
  network_config::BindToInProcessInstance(
      cros_network_config_.BindNewPipeAndPassReceiver());
  cros_network_config_->AddObserver(receiver_.BindNewPipeAndPassRemote());

  phone_model_->AddObserver(this);
  multidevice_setup_client_->AddObserver(this);

  // Compute current status.
  status_ = ComputeStatus();

  // Load the current tether network if it exists.
  FetchVisibleTetherNetwork();
}

TetherControllerImpl::~TetherControllerImpl() {
  phone_model_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);
}

TetherController::Status TetherControllerImpl::GetStatus() const {
  PA_LOG(VERBOSE) << __func__ << ": status = " << status_;
  return status_;
}

void TetherControllerImpl::ScanForAvailableConnection() {
  if (status_ != Status::kConnectionUnavailable) {
    PA_LOG(WARNING) << "Received request to scan for available connection, but "
                    << "a scan cannot be performed because the current status "
                    << "is " << status_;
    return;
  }

  PA_LOG(INFO) << "Scanning for available connection.";
  cros_network_config_->RequestNetworkScan(NetworkType::kTether);
}

void TetherControllerImpl::AttemptConnection() {
  if (status_ != Status::kConnectionUnavailable &&
      status_ != Status::kConnectionAvailable) {
    PA_LOG(WARNING) << "Received request to attempt a connection, but a "
                    << "connection cannot be attempted because the current "
                    << "status is " << status_;
    return;
  }

  PA_LOG(INFO) << "Attempting connection; current status is " << status_;
  user_action_recorder_->RecordTetherConnectionAttempt();
  util::LogTetherConnectionResult(
      util::TetherConnectionResult::kAttemptConnection);
  is_attempting_connection_ = true;

  FeatureState feature_state =
      multidevice_setup_client_->GetFeatureState(Feature::kInstantTethering);

  if (feature_state == FeatureState::kEnabledByUser) {
    PerformConnectionAttempt();
    return;
  }

  // The Tethering feature was disabled and must be enabled first, before a
  // connection attempt can be made.
  DCHECK(feature_state == FeatureState::kDisabledByUser);
  AttemptTurningOnTethering();
}

void TetherControllerImpl::AttemptTurningOnTethering() {
  SetConnectDisconnectStatus(
      ConnectDisconnectStatus::kTurningOnInstantTethering);
  multidevice_setup_client_->SetFeatureEnabledState(
      Feature::kInstantTethering,
      /*enabled=*/true,
      /*auth_token=*/std::nullopt,
      base::BindOnce(&TetherControllerImpl::OnSetFeatureEnabled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TetherControllerImpl::OnSetFeatureEnabled(bool success) {
  if (connect_disconnect_status_ !=
      ConnectDisconnectStatus::kTurningOnInstantTethering) {
    return;
  }

  if (success) {
    PerformConnectionAttempt();
    return;
  }

  PA_LOG(WARNING) << "Failed to enable InstantTethering";
  SetConnectDisconnectStatus(ConnectDisconnectStatus::kIdle);
}

void TetherControllerImpl::OnFeatureStatesChanged(
    const MultiDeviceSetupClient::FeatureStatesMap& feature_states_map) {
  FeatureState feature_state =
      multidevice_setup_client_->GetFeatureState(Feature::kInstantTethering);

  // The |connect_disconnect_status_| should always be
  // ConnectDisconnectStatus::kIdle if the |feature_state| is anything other
  // than |FeatureState::kEnabledByUser|. A |feature_status| other than
  // |FeatureState::kEnabledByUser| would indicate that Instant Tethering became
  // disabled or disallowed.
  if (feature_state != FeatureState::kEnabledByUser) {
    SetConnectDisconnectStatus(ConnectDisconnectStatus::kIdle);
  } else if (connect_disconnect_status_ !=
             ConnectDisconnectStatus::kTurningOnInstantTethering) {
    UpdateStatus();
  }
}

void TetherControllerImpl::PerformConnectionAttempt() {
  if (!tether_network_.is_null()) {
    StartConnect();
    return;
  }
  SetConnectDisconnectStatus(
      ConnectDisconnectStatus::kScanningForEligiblePhone);
  cros_network_config_->RequestNetworkScan(NetworkType::kTether);
}

void TetherControllerImpl::StartConnect() {
  DCHECK(!tether_network_.is_null());
  SetConnectDisconnectStatus(
      ConnectDisconnectStatus::kConnectingToEligiblePhone);
  connector_->StartConnect(
      tether_network_->guid,
      base::BindOnce(&TetherControllerImpl::OnStartConnectCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TetherControllerImpl::OnStartConnectCompleted(StartConnectResult result,
                                                   const std::string& message) {
  if (result != StartConnectResult::kSuccess) {
    PA_LOG(WARNING) << "Start connect failed with result " << result
                    << " and message " << message;
  }

  if (connect_disconnect_status_ !=
      ConnectDisconnectStatus::kConnectingToEligiblePhone) {
    return;
  }

  // Note that OnVisibleTetherNetworkFetched() may not have called
  // SetConnectDisconnectStatus() with kIdle at this point, so this should go
  // ahead and do it.
  SetConnectDisconnectStatus(ConnectDisconnectStatus::kIdle);
}

void TetherControllerImpl::Disconnect() {
  if (status_ != Status::kConnecting && status_ != Status::kConnected) {
    PA_LOG(WARNING) << "Received request to disconnect, but no connection or "
                    << "connection attempt is in progress. Current status is "
                    << status_;
    return;
  }

  // If |status_| is Status::kConnecting, a tether network may not be available
  // yet e.g this class may be in the process of enabling Instant Tethering.
  if (tether_network_.is_null()) {
    SetConnectDisconnectStatus(ConnectDisconnectStatus::kIdle);
    return;
  }

  PA_LOG(INFO) << "Attempting disconnection; current status is " << status_;
  SetConnectDisconnectStatus(ConnectDisconnectStatus::kDisconnecting);
  connector_->StartDisconnect(
      tether_network_->guid,
      base::BindOnce(&TetherControllerImpl::OnDisconnectCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TetherControllerImpl::OnModelChanged() {
  UpdateStatus();
}

void TetherControllerImpl::OnDisconnectCompleted(bool success) {
  if (connect_disconnect_status_ != ConnectDisconnectStatus::kDisconnecting)
    return;

  SetConnectDisconnectStatus(ConnectDisconnectStatus::kIdle);

  // Fetch the tether network and its updated connection state, if it exists.
  // By the time OnDisconnectCompleted() is called, the connection state is
  // properly updated to ConnectionStateType::kDisconnected, so a fetch may be
  // necessary to promptly update |tether_network_|, as neither
  // OnActiveNetworksChanged() nor OnNetworkStateListChanged() may be called
  // shortly afterwards with the latest network information.
  FetchVisibleTetherNetwork();

  if (!success)
    PA_LOG(WARNING) << "Failed to disconnect tether network";
}

void TetherControllerImpl::OnActiveNetworksChanged(
    std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
        networks) {
  // Active networks either changed externally (e.g via OS Settings or a new
  // actve network added), or as a result of a call to AttemptConnection() or
  // Disconnect(). This is needed for the case of
  // ConnectionStateType::kConnecting in ComputeStatus().
  //
  // Note: When OnActiveNetworksChanged() is called shortly after starting a
  // disconnect to a ConnectionStateType::kConnecting |tether_network_|, the
  // |tether_network_|'s ConnectionStateType may still remain in the
  // ConnectionStateType::kConnecting state. This may happen if on the phone,
  // hotspot is off but bluetooth tethering is on, and a connection attempt is
  // made, but the user does not acknowledge the notification to connect on
  // their phone, and subsequently decides to disconnect while
  // |tether_network_|'s ConnectionStateType is still
  // ConnectionStateType::kConnecting.
  FetchVisibleTetherNetwork();
}

void TetherControllerImpl::OnNetworkStateListChanged() {
  // Any network change whether caused externally or within this class should
  // be reflected to the state of this class (e.g user makes changes to Tether
  // network in OS Settings).
  FetchVisibleTetherNetwork();
}

void TetherControllerImpl::OnDeviceStateListChanged() {
  if (connect_disconnect_status_ !=
      ConnectDisconnectStatus::kScanningForEligiblePhone) {
    return;
  }

  cros_network_config_->GetDeviceStateList(
      base::BindOnce(&TetherControllerImpl::OnGetDeviceStateList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TetherControllerImpl::OnGetDeviceStateList(
    std::vector<DeviceStatePropertiesPtr> devices) {
  if (connect_disconnect_status_ !=
      ConnectDisconnectStatus::kScanningForEligiblePhone) {
    return;
  }

  // There should only be one Tether device in the list.
  bool is_tether_device_scanning = false;
  for (const auto& device : devices) {
    NetworkType type = device->type;
    if (type != NetworkType::kTether)
      continue;
    is_tether_device_scanning = device->scanning;
    break;
  }

  if (!is_tether_device_scanning) {
    NotifyAttemptConnectionScanFailed();
    SetConnectDisconnectStatus(ConnectDisconnectStatus::kIdle);
  }
}

void TetherControllerImpl::FetchVisibleTetherNetwork() {
  // Return the connected, connecting, or connectable Tether network.
  connector_->GetNetworkStateList(
      chromeos::network_config::mojom::NetworkFilter::New(FilterType::kVisible,
                                                          NetworkType::kTether,
                                                          /*limit=*/0),
      base::BindOnce(&TetherControllerImpl::OnVisibleTetherNetworkFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TetherControllerImpl::OnVisibleTetherNetworkFetched(
    std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
        networks) {
  chromeos::network_config::mojom::NetworkStatePropertiesPtr
      previous_tether_network = std::move(tether_network_);

  if (!networks.empty()) {
    // The number of tether networks is expected to be at most 1, though some
    // tests do use multiple networks.
    tether_network_ = std::move(networks[0]);
  } else {
    tether_network_ = nullptr;
  }

  // No observeable changes to the tether network specifically. This fetch
  // was initiated by a change in a non Tether network type.
  if (tether_network_.Equals(previous_tether_network))
    return;

  // If AttemptConnection() was called when Instant Tethering was disabled.
  // The feature must be enabled before scanning can occur.
  if (connect_disconnect_status_ ==
      ConnectDisconnectStatus::kTurningOnInstantTethering) {
    UpdateStatus();
    return;
  }

  // If AttemptConnection() was called when there was no available tether
  // connection.
  if (connect_disconnect_status_ ==
          ConnectDisconnectStatus::kScanningForEligiblePhone &&
      !tether_network_.is_null()) {
    StartConnect();
    return;
  }

  // If there is no attempt connection in progress, or an attempt connection
  // caused OnVisibleTetherNetworkFetched() to be fired. This case also occurs
  // in the event that Tethering settings are changed externally from this class
  // (e.g user connects via Settings).
  SetConnectDisconnectStatus(ConnectDisconnectStatus::kIdle);
}

void TetherControllerImpl::SetConnectDisconnectStatus(
    ConnectDisconnectStatus connect_disconnect_status) {
  if (connect_disconnect_status_ != connect_disconnect_status)
    weak_ptr_factory_.InvalidateWeakPtrs();
  connect_disconnect_status_ = connect_disconnect_status;
  UpdateStatus();
}

void TetherControllerImpl::UpdateStatus() {
  Status status = ComputeStatus();

  if (status_ == status)
    return;

  PA_LOG(INFO) << "TetherController status update: " << status_ << " => "
               << status;

  status_ = status;

  if (is_attempting_connection_ && status_ == Status::kConnected)
    util::LogTetherConnectionResult(util::TetherConnectionResult::kSuccess);

  if (status_ != Status::kConnecting)
    is_attempting_connection_ = false;

  NotifyStatusChanged();
}

TetherController::Status TetherControllerImpl::ComputeStatus() const {
  FeatureState feature_state =
      multidevice_setup_client_->GetFeatureState(Feature::kInstantTethering);

  if (feature_state != FeatureState::kDisabledByUser &&
      feature_state != FeatureState::kEnabledByUser) {
    // Tethering may be for instance, prohibited by policy or not supported
    // by the phone or Chromebook.
    return Status::kIneligibleForFeature;
  }

  if (phone_model_->phone_status_model().has_value()) {
    // If the phone status exists, and it indicates that the phone
    // does not have reception, the status becomes no kNoReception.
    bool does_sim_exist_with_reception =
        phone_model_->phone_status_model()->mobile_status() ==
        PhoneStatusModel::MobileStatus::kSimWithReception;

    if (!does_sim_exist_with_reception)
      return Status::kNoReception;
  }

  if (connect_disconnect_status_ ==
          ConnectDisconnectStatus::kTurningOnInstantTethering ||
      connect_disconnect_status_ ==
          ConnectDisconnectStatus::kScanningForEligiblePhone ||
      connect_disconnect_status_ ==
          ConnectDisconnectStatus::kConnectingToEligiblePhone) {
    return Status::kConnecting;
  }

  if (feature_state == FeatureState::kDisabledByUser)
    return Status::kConnectionUnavailable;

  if (tether_network_.is_null())
    return Status::kConnectionUnavailable;

  ConnectionStateType connection_state = tether_network_->connection_state;

  switch (connection_state) {
    case ConnectionStateType::kOnline:
      [[fallthrough]];
    case ConnectionStateType::kConnected:
      [[fallthrough]];
    case ConnectionStateType::kPortal:
      return Status::kConnected;

    case ConnectionStateType::kConnecting:
      return Status::kConnecting;

    case ConnectionStateType::kNotConnected:
      return Status::kConnectionAvailable;
  }
  return Status::kConnectionUnavailable;
}

}  // namespace phonehub
}  // namespace ash
