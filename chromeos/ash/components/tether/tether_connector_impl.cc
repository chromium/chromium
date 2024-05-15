// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_connector_impl.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/tether/active_host.h"
#include "chromeos/ash/components/tether/connect_tethering_operation.h"
#include "chromeos/ash/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/ash/components/tether/disconnect_tethering_request_sender.h"
#include "chromeos/ash/components/tether/host_connection_metrics_logger.h"
#include "chromeos/ash/components/tether/host_scan_cache.h"
#include "chromeos/ash/components/tether/notification_presenter.h"
#include "chromeos/ash/components/tether/tether_host_fetcher.h"
#include "chromeos/ash/components/tether/tether_host_response_recorder.h"
#include "chromeos/ash/components/tether/wifi_hotspot_connector.h"
#include "chromeos/ash/components/tether/wifi_hotspot_disconnector.h"

namespace ash::tether {

using ConnectionToHostResult =
    HostConnectionMetricsLogger::ConnectionToHostResult;
using ConnectionToHostInternalError =
    HostConnectionMetricsLogger::ConnectionToHostInternalError;

namespace {

void OnDisconnectFromWifiFailure(const std::string& device_id,
                                 const std::string& error_name) {
  PA_LOG(WARNING) << "Failed to disconnect from tether hotspot for device ID "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         device_id)
                  << ". Error: " << error_name;
}

std::pair<ConnectionToHostResult, std::optional<ConnectionToHostInternalError>>
GetConnectionToHostResponseAndInternalErrorFromWifiHotspotConnectionError(
    WifiHotspotConnector::WifiHotspotConnectionError error) {
  switch (error) {
    case WifiHotspotConnector::WifiHotspotConnectionError::kTimeout:
      return std::make_pair(
          ConnectionToHostResult::INTERNAL_ERROR,
          ConnectionToHostInternalError::CLIENT_CONNECTION_TIMEOUT);
    case WifiHotspotConnector::WifiHotspotConnectionError::
        kCancelledForNewerConnectionAttempt:
      return std::make_pair(
          ConnectionToHostResult::CANCELLED_FOR_NEWER_CONNECTION, std::nullopt);
    case WifiHotspotConnector::WifiHotspotConnectionError::
        kWifiHotspotConnectorClassDestroyed:
      // It's currently safe to assume TetherComponent shut down in this
      // situation, since that's the only way the Connector class could be
      // destroyed. This may change in the future.
      return std::make_pair(
          ConnectionToHostResult::TETHER_SHUTDOWN_DURING_CONNECTION,
          std::nullopt);
    case WifiHotspotConnector::WifiHotspotConnectionError::kNetworkStateWasNull:
      return std::make_pair(ConnectionToHostResult::INTERNAL_ERROR,
                            ConnectionToHostInternalError::
                                CLIENT_CONNECTION_NETWORK_STATE_WAS_NULL);
    case WifiHotspotConnector::WifiHotspotConnectionError::
        kNetworkConnectionHandlerFailed:
      return std::make_pair(
          ConnectionToHostResult::INTERNAL_ERROR,
          ConnectionToHostInternalError::
              CLIENT_CONNECTION_NETWORK_CONNECTION_HANDLER_FAILED);
    case WifiHotspotConnector::WifiHotspotConnectionError::kWifiFailedToEnabled:
      return std::make_pair(ConnectionToHostResult::INTERNAL_ERROR,
                            ConnectionToHostInternalError::
                                CLIENT_CONNECTION_WIFI_FAILED_TO_ENABLE);
  }
}

}  // namespace

TetherConnectorImpl::TetherConnectorImpl(
    raw_ptr<HostConnection::Factory> host_connection_factory,
    NetworkStateHandler* network_state_handler,
    WifiHotspotConnector* wifi_hotspot_connector,
    ActiveHost* active_host,
    TetherHostFetcher* tether_host_fetcher,
    TetherHostResponseRecorder* tether_host_response_recorder,
    DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map,
    HostScanCache* host_scan_cache,
    NotificationPresenter* notification_presenter,
    HostConnectionMetricsLogger* host_connection_metrics_logger,
    DisconnectTetheringRequestSender* disconnect_tethering_request_sender,
    WifiHotspotDisconnector* wifi_hotspot_disconnector)
    : host_connection_factory_(host_connection_factory),
      network_state_handler_(network_state_handler),
      wifi_hotspot_connector_(wifi_hotspot_connector),
      active_host_(active_host),
      tether_host_fetcher_(tether_host_fetcher),
      tether_host_response_recorder_(tether_host_response_recorder),
      device_id_tether_network_guid_map_(device_id_tether_network_guid_map),
      host_scan_cache_(host_scan_cache),
      notification_presenter_(notification_presenter),
      host_connection_metrics_logger_(host_connection_metrics_logger),
      disconnect_tethering_request_sender_(disconnect_tethering_request_sender),
      wifi_hotspot_disconnector_(wifi_hotspot_disconnector) {}

TetherConnectorImpl::~TetherConnectorImpl() {
  if (connect_tethering_operation_) {
    connect_tethering_operation_->RemoveObserver(this);
  }
}

void TetherConnectorImpl::ConnectToNetwork(
    const std::string& tether_network_guid,
    base::OnceClosure success_callback,
    StringErrorCallback error_callback) {
  DCHECK(!tether_network_guid.empty());
  DCHECK(!success_callback.is_null());
  DCHECK(!error_callback.is_null());

  PA_LOG(VERBOSE) << "Attempting to connect to network with GUID "
                  << tether_network_guid << ".";
  notification_presenter_->RemoveConnectionToHostFailedNotification();

  const std::string device_id =
      device_id_tether_network_guid_map_->GetDeviceIdForTetherNetworkGuid(
          tether_network_guid);

  // If NetworkConnectionHandler receives a connection request for a network
  // to which it is already attempting a connection, it should stop the
  // duplicate connection request itself before invoking its TetherDelegate.
  // Thus, ConnectToNetwork() should never be called for a device which is
  // already pending connection.
  DCHECK(device_id_pending_connection_ != device_id);

  if (!device_id_pending_connection_.empty()) {
    PA_LOG(VERBOSE) << "A connection attempt was already in progress to device "
                    << "with ID " << device_id_pending_connection_ << ". "
                    << "Canceling that connection attempt before continuing.";
    CancelConnectionAttempt(
        device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
            device_id_pending_connection_));
  }

  device_id_pending_connection_ = device_id;
  success_callback_ = std::move(success_callback);
  error_callback_ = std::move(error_callback);
  active_host_->SetActiveHostConnecting(device_id, tether_network_guid);

  std::optional<multidevice::RemoteDeviceRef> tether_host_to_connect =
      tether_host_fetcher_->GetTetherHost();

  if (!tether_host_to_connect ||
      device_id != tether_host_to_connect->GetDeviceId()) {
    PA_LOG(ERROR) << "Could not fetch tether host with device ID "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         device_id)
                  << ". Cannot connect.";
    host_connection_metrics_logger_->RecordConnectionToHostResult(
        ConnectionToHostResult::INTERNAL_ERROR, device_id_pending_connection_,
        ConnectionToHostInternalError::CLIENT_CONNECTION_INTERNAL_ERROR);
    SetConnectionFailed(NetworkConnectionHandler::kErrorConnectFailed);
    return;
  }

  connect_tethering_operation_ = ConnectTetheringOperation::Factory::Create(
      TetherHost(*tether_host_to_connect), host_connection_factory_,
      host_scan_cache_->DoesHostRequireSetup(tether_network_guid));
  connect_tethering_operation_->AddObserver(this);
  connect_tethering_operation_->Initialize();
}

bool TetherConnectorImpl::CancelConnectionAttempt(
    const std::string& tether_network_guid) {
  const std::string device_id =
      device_id_tether_network_guid_map_->GetDeviceIdForTetherNetworkGuid(
          tether_network_guid);

  if (device_id != device_id_pending_connection_) {
    PA_LOG(ERROR) << "CancelConnectionAttempt(): Cancel requested for Tether "
                  << "network with GUID " << tether_network_guid << ", but "
                  << "there was no active connection to that network.";
    return false;
  }

  PA_LOG(VERBOSE) << "Canceling connection attempt to Tether network with GUID "
                  << tether_network_guid;

  if (connect_tethering_operation_) {
    // If a ConnectTetheringOperation is in progress, stop it.
    connect_tethering_operation_->RemoveObserver(this);
    connect_tethering_operation_.reset();
  }

  // Send a DisconnectTetheringRequest so that it can turn off its Wi-Fi
  // hotspot.
  disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
      device_id);

  host_connection_metrics_logger_->RecordConnectionToHostResult(
      ConnectionToHostResult::USER_CANCELLATION, device_id_pending_connection_,
      std::nullopt);

  SetConnectionFailed(NetworkConnectionHandler::kErrorConnectCanceled);
  return true;
}

void TetherConnectorImpl::OnConnectTetheringRequestSent() {
  did_send_successful_request_ = true;

  // If setup is required for the phone, display a notification so that the
  // user knows to follow instructions on the phone. Note that the notification
  // is displayed only after a request has been sent successfully. If the
  // notification is displayed before a the request has been sent, it could be
  // misleading since the connection could fail. See crbug.com/767756.
  const std::string tether_network_guid =
      device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
          device_id_pending_connection_);
  if (!host_scan_cache_->DoesHostRequireSetup(tether_network_guid)) {
    return;
  }

  const NetworkState* tether_network_state =
      network_state_handler_->GetNetworkStateFromGuid(tether_network_guid);
  DCHECK(tether_network_state);
  notification_presenter_->NotifySetupRequired(
      tether_network_state->name(), tether_network_state->signal_strength());
}

void TetherConnectorImpl::OnSuccessfulConnectTetheringResponse(
    const std::string& ssid,
    const std::string& password) {
  tether_host_response_recorder_->RecordSuccessfulConnectTetheringResponse(
      device_id_pending_connection_);

  PA_LOG(VERBOSE) << "Received successful ConnectTetheringResponse from device "
                  << "with ID "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         device_id_pending_connection_)
                  << ". SSID: \"" << ssid << "\".";

  // Make a copy of the device ID, SSID, and password to pass below before
  // destroying |connect_tethering_operation_|.
  std::string ssid_copy = ssid;
  std::string password_copy = password;

  connect_tethering_operation_->RemoveObserver(this);
  connect_tethering_operation_.reset();

  wifi_hotspot_connector_->ConnectToWifiHotspot(
      ssid_copy, password_copy, active_host_->GetTetherNetworkGuid(),
      base::BindOnce(&TetherConnectorImpl::OnWifiConnection,
                     weak_ptr_factory_.GetWeakPtr(),
                     device_id_pending_connection_));
}

void TetherConnectorImpl::OnConnectTetheringFailure(
    ConnectTetheringOperation::HostResponseErrorCode error_code) {
  PA_LOG(WARNING) << "Connection to device with ID "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         device_id_pending_connection_)
                  << " could not complete. Error code: " << error_code;

  connect_tethering_operation_->RemoveObserver(this);
  connect_tethering_operation_.reset();
  RecordConnectTetheringOperationResult(device_id_pending_connection_,
                                        error_code);
  SetConnectionFailed(NetworkConnectionHandler::kErrorConnectFailed);
}

void TetherConnectorImpl::SetConnectionFailed(const std::string& error_name) {
  DCHECK(!device_id_pending_connection_.empty());
  DCHECK(!error_callback_.is_null());

  notification_presenter_->RemoveSetupRequiredNotification();

  // Save a copy of the callback before resetting it below.
  StringErrorCallback error_callback = std::move(error_callback_);

  std::string failed_connection_device_id = device_id_pending_connection_;
  device_id_pending_connection_.clear();

  success_callback_.Reset();
  error_callback_.Reset();

  std::move(error_callback).Run(error_name);
  active_host_->SetActiveHostDisconnected();

  if (error_name == NetworkConnectionHandler::kErrorConnectFailed) {
    // Only show notification if the error is kErrorConnectFailed. Other error
    // names (e.g., kErrorConnectCanceled) are a result of user interaction and
    // should not result in any error UI.
    notification_presenter_->NotifyConnectionToHostFailed();
  }
}

void TetherConnectorImpl::SetConnectionSucceeded(
    const std::string& device_id,
    const std::string& wifi_network_guid) {
  DCHECK(!device_id_pending_connection_.empty());
  DCHECK(device_id_pending_connection_ == device_id);
  DCHECK(!success_callback_.is_null());

  host_connection_metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::SUCCESS, device_id,
      std::nullopt);

  notification_presenter_->RemoveSetupRequiredNotification();

  // Save the callback before resetting it below.
  base::OnceClosure success_callback = std::move(success_callback_);

  device_id_pending_connection_.clear();
  success_callback_.Reset();
  error_callback_.Reset();

  std::move(success_callback).Run();
  active_host_->SetActiveHostConnected(
      device_id,
      device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
          device_id),
      wifi_network_guid);
}

void TetherConnectorImpl::OnWifiConnection(
    const std::string& device_id,
    base::expected<std::string,
                   WifiHotspotConnector::WifiHotspotConnectionError>
        wifi_network_guid) {
  if (device_id != device_id_pending_connection_) {
    if (!wifi_network_guid.has_value()) {
      PA_LOG(WARNING)
          << "Failed to connect to Wi-Fi hotspot for device with ID "
          << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(device_id)
          << ", " << "but the connection to that device was canceled.";
      return;
    }

    PA_LOG(VERBOSE) << "Connected to Wi-Fi hotspot for device with ID "
                    << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                           device_id)
                    << ", but the connection to that device failed. "
                    << "Disconnecting.";

    // Disconnect from the Wi-Fi hotspot; otherwise, it is possible to be
    // connected to the Wi-Fi hotspot despite there being no active host. See
    // crbug.com/761171.
    wifi_hotspot_disconnector_->DisconnectFromWifiHotspot(
        wifi_network_guid.value(), base::DoNothing(),
        base::BindOnce(&OnDisconnectFromWifiFailure, device_id));
    return;
  }

  if (!wifi_network_guid.has_value()) {
    // If the Wi-Fi network ID is empty, then the connection did not succeed.
    PA_LOG(ERROR) << "Failed to connect to the hotspot belonging to the device "
                  << "with ID "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         device_id)
                  << ".";

    auto connection_to_host_result_and_internal_error =
        GetConnectionToHostResponseAndInternalErrorFromWifiHotspotConnectionError(
            wifi_network_guid.error());

    host_connection_metrics_logger_->RecordConnectionToHostResult(
        connection_to_host_result_and_internal_error.first, device_id,
        connection_to_host_result_and_internal_error.second);
    SetConnectionFailed(NetworkConnectionHandler::kErrorConnectFailed);
    return;
  }

  SetConnectionSucceeded(device_id, wifi_network_guid.value());
}

void TetherConnectorImpl::RecordConnectTetheringOperationResult(
    const std::string& device_id,
    ConnectTetheringOperation::HostResponseErrorCode error_code) {
  std::optional<ConnectionToHostResult> result =
      ConnectionToHostResult::INTERNAL_ERROR;
  std::optional<ConnectionToHostInternalError> internal_error = std::nullopt;

  switch (error_code) {
    case ConnectTetheringOperation::HostResponseErrorCode::PROVISIONING_FAILED:
      result = ConnectionToHostResult::PROVISIONING_FAILURE;
      break;
    case ConnectTetheringOperation::HostResponseErrorCode::TETHERING_TIMEOUT:
      if (host_scan_cache_->DoesHostRequireSetup(
              device_id_tether_network_guid_map_
                  ->GetTetherNetworkGuidForDeviceId(device_id))) {
        internal_error = ConnectionToHostInternalError::
            TETHERING_TIMED_OUT_FIRST_TIME_SETUP_REQUIRED;
      } else {
        internal_error = ConnectionToHostInternalError::
            TETHERING_TIMED_OUT_FIRST_TIME_SETUP_NOT_REQUIRED;
      }
      break;
    case ConnectTetheringOperation::HostResponseErrorCode::
        TETHERING_UNSUPPORTED:
      result = ConnectionToHostResult::TETHERING_UNSUPPORTED;
      break;
    case ConnectTetheringOperation::HostResponseErrorCode::NO_CELL_DATA:
      result = ConnectionToHostResult::NO_CELLULAR_DATA;
      break;
    case ConnectTetheringOperation::HostResponseErrorCode::
        ENABLING_HOTSPOT_FAILED:
      internal_error = ConnectionToHostInternalError::ENABLING_HOTSPOT_FAILED;
      break;
    case ConnectTetheringOperation::HostResponseErrorCode::
        ENABLING_HOTSPOT_TIMEOUT:
      internal_error = ConnectionToHostInternalError::ENABLING_HOTSPOT_TIMEOUT;
      break;
    case ConnectTetheringOperation::HostResponseErrorCode::UNKNOWN_ERROR:
      internal_error = ConnectionToHostInternalError::UNKNOWN_ERROR;
      break;
    case ConnectTetheringOperation::HostResponseErrorCode::
        INVALID_HOTSPOT_CREDENTIALS:
      internal_error =
          ConnectionToHostInternalError::INVALID_HOTSPOT_CREDENTIALS;
      break;
    case ConnectTetheringOperation::HostResponseErrorCode::NO_RESPONSE:
      if (did_send_successful_request_) {
        internal_error =
            ConnectionToHostInternalError::SUCCESSFUL_REQUEST_BUT_NO_RESPONSE;
      } else {
        internal_error = ConnectionToHostInternalError::NO_RESPONSE;
      }
      break;
    case ConnectTetheringOperation::HostResponseErrorCode::
        INVALID_ACTIVE_EXISTING_SOFT_AP_CONFIG:
      internal_error =
          ConnectionToHostInternalError::INVALID_ACTIVE_EXISTING_SOFT_AP_CONFIG;
      break;
    case ConnectTetheringOperation::HostResponseErrorCode::
        INVALID_NEW_SOFT_AP_CONFIG:
      internal_error =
          ConnectionToHostInternalError::INVALID_NEW_SOFT_AP_CONFIG;
      break;
    case ConnectTetheringOperation::HostResponseErrorCode::
        INVALID_WIFI_AP_CONFIG:
      internal_error = ConnectionToHostInternalError::INVALID_WIFI_AP_CONFIG;
      break;
    default:
      internal_error =
          ConnectionToHostInternalError::UNRECOGNIZED_RESPONSE_ERROR;
      break;
  }

  if (result.has_value()) {
    host_connection_metrics_logger_->RecordConnectionToHostResult(
        result.value(), device_id, internal_error);
  }
}

}  // namespace ash::tether
