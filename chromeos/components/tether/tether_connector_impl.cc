// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_connector_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/tether/active_host.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/disconnect_tethering_request_sender.h"
#include "chromeos/components/tether/host_connection_metrics_logger.h"
#include "chromeos/components/tether/host_scan_cache.h"
#include "chromeos/components/tether/notification_presenter.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "chromeos/components/tether/wifi_hotspot_connector.h"
#include "chromeos/components/tether/wifi_hotspot_disconnector.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"

namespace chromeos {

namespace tether {

namespace {

void OnDisconnectFromWifiFailure(const std::string& device_id,
                                 const std::string& error_name) {
  PA_LOG(WARNING) << "Failed to disconnect from tether hotspot for device ID "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         device_id)
                  << ". Error: " << error_name;
}

}  // namespace

TetherConnectorImpl::TetherConnectorImpl(
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
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
    : device_sync_client_(device_sync_client),
      secure_channel_client_(secure_channel_client),
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
  if (connect_tethering_operation_)
    connect_tethering_operation_->RemoveObserver(this);
}

void TetherConnectorImpl::ConnectToNetwork(
    const std::string& tether_network_guid,
    const base::Closure& success_callback,
    const network_handler::StringResultCallback& error_callback) {
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
  success_callback_ = success_callback;
  error_callback_ = error_callback;
  active_host_->SetActiveHostConnecting(device_id, tether_network_guid);

  tether_host_fetcher_->FetchTetherHost(
      device_id_pending_connection_,
      base::Bind(&TetherConnectorImpl::OnTetherHostToConnectFetched,
                 weak_ptr_factory_.GetWeakPtr(),
                 device_id_pending_connection_));
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

  SetConnectionFailed(
      NetworkConnectionHandler::kErrorConnectCanceled,
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_CANCELED_BY_USER);
  return true;
}

void TetherConnectorImpl::OnConnectTetheringRequestSent(
    multidevice::RemoteDeviceRef remote_device) {
  did_send_successful_request_ = true;

  // If setup is required for the phone, display a notification so that the
  // user knows to follow instructions on the phone. Note that the notification
  // is displayed only after a request has been sent successfully. If the
  // notification is displayed before a the request has been sent, it could be
  // misleading since the connection could fail. See crbug.com/767756.
  const std::string tether_network_guid =
      device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
          remote_device.GetDeviceId());
  if (!host_scan_cache_->DoesHostRequireSetup(tether_network_guid))
    return;

  const NetworkState* tether_network_state =
      network_state_handler_->GetNetworkStateFromGuid(tether_network_guid);
  DCHECK(tether_network_state);
  notification_presenter_->NotifySetupRequired(
      tether_network_state->name(), tether_network_state->signal_strength());
}

void TetherConnectorImpl::OnSuccessfulConnectTetheringResponse(
    multidevice::RemoteDeviceRef remote_device,
    const std::string& ssid,
    const std::string& password) {
  if (device_id_pending_connection_ != remote_device.GetDeviceId()) {
    // If the success was part of a previous attempt for a different device,
    // ignore it.
    PA_LOG(VERBOSE) << "Received successful ConnectTetheringResponse from "
                    << "device with ID "
                    << remote_device.GetTruncatedDeviceIdForLogs()
                    << ", but the "
                    << "connection attempt to that device has been canceled.";

    return;
  }

  PA_LOG(VERBOSE) << "Received successful ConnectTetheringResponse from device "
                  << "with ID " << remote_device.GetTruncatedDeviceIdForLogs()
                  << ". SSID: \"" << ssid << "\".";

  // Make a copy of the device ID, SSID, and password to pass below before
  // destroying |connect_tethering_operation_|.
  std::string remote_device_id = remote_device.GetDeviceId();
  std::string ssid_copy = ssid;
  std::string password_copy = password;

  connect_tethering_operation_->RemoveObserver(this);
  connect_tethering_operation_.reset();

  wifi_hotspot_connector_->ConnectToWifiHotspot(
      ssid_copy, password_copy, active_host_->GetTetherNetworkGuid(),
      base::Bind(&TetherConnectorImpl::OnWifiConnection,
                 weak_ptr_factory_.GetWeakPtr(), remote_device_id));
}

void TetherConnectorImpl::OnConnectTetheringFailure(
    multidevice::RemoteDeviceRef remote_device,
    ConnectTetheringOperation::HostResponseErrorCode error_code) {
  std::string device_id_copy = remote_device.GetDeviceId();
  if (device_id_pending_connection_ != device_id_copy) {
    // If the failure was part of a previous attempt for a different device,
    // ignore it.
    PA_LOG(VERBOSE)
        << "Received failed ConnectTetheringResponse from device with "
        << "ID " << remote_device.GetTruncatedDeviceIdForLogs()
        << ", but a connection to another device has already started.";
    return;
  }

  PA_LOG(WARNING) << "Connection to device with ID "
                  << remote_device.GetTruncatedDeviceIdForLogs()
                  << " could not complete. Error code: " << error_code;

  connect_tethering_operation_->RemoveObserver(this);
  connect_tethering_operation_.reset();
  SetConnectionFailed(
      NetworkConnectionHandler::kErrorConnectFailed,
      GetConnectionToHostResultFromErrorCode(device_id_copy, error_code));
}

void TetherConnectorImpl::OnTetherHostToConnectFetched(
    const std::string& device_id,
    base::Optional<multidevice::RemoteDeviceRef> tether_host_to_connect) {
  if (device_id_pending_connection_ != device_id) {
    PA_LOG(VERBOSE) << "Device to connect to has changed while device with ID "
                    << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                           device_id)
                    << " was being fetched.";
    return;
  }

  if (!tether_host_to_connect) {
    PA_LOG(ERROR) << "Could not fetch tether host with device ID "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         device_id)
                  << ". Cannot connect.";
    SetConnectionFailed(
        NetworkConnectionHandler::kErrorConnectFailed,
        HostConnectionMetricsLogger::ConnectionToHostResult::
            CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_INTERNAL_ERROR);
    return;
  }

  DCHECK(device_id == tether_host_to_connect->GetDeviceId());

  const std::string tether_network_guid =
      device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
          device_id);
  connect_tethering_operation_ =
      ConnectTetheringOperation::Factory::NewInstance(
          *tether_host_to_connect, device_sync_client_, secure_channel_client_,
          tether_host_response_recorder_,
          host_scan_cache_->DoesHostRequireSetup(tether_network_guid));
  connect_tethering_operation_->AddObserver(this);
  connect_tethering_operation_->Initialize();
}

void TetherConnectorImpl::SetConnectionFailed(
    const std::string& error_name,
    HostConnectionMetricsLogger::ConnectionToHostResult
        connection_to_host_result) {
  DCHECK(!device_id_pending_connection_.empty());
  DCHECK(!error_callback_.is_null());

  notification_presenter_->RemoveSetupRequiredNotification();

  // Save a copy of the callback before resetting it below.
  network_handler::StringResultCallback error_callback = error_callback_;

  std::string failed_connection_device_id = device_id_pending_connection_;
  device_id_pending_connection_.clear();

  success_callback_.Reset();
  error_callback_.Reset();

  error_callback.Run(error_name);
  active_host_->SetActiveHostDisconnected();

  host_connection_metrics_logger_->RecordConnectionToHostResult(
      connection_to_host_result, failed_connection_device_id);

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
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_SUCCESS,
      device_id);

  notification_presenter_->RemoveSetupRequiredNotification();

  // Save a copy of the callback before resetting it below.
  base::Closure success_callback = success_callback_;

  device_id_pending_connection_.clear();
  success_callback_.Reset();
  error_callback_.Reset();

  success_callback.Run();
  active_host_->SetActiveHostConnected(
      device_id,
      device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
          device_id),
      wifi_network_guid);
}

void TetherConnectorImpl::OnWifiConnection(
    const std::string& device_id,
    const std::string& wifi_network_guid) {
  if (device_id != device_id_pending_connection_) {
    if (wifi_network_guid.empty()) {
      PA_LOG(WARNING)
          << "Failed to connect to Wi-Fi hotspot for device with ID "
          << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(device_id)
          << ", "
          << "but the connection to that device was canceled.";
      return;
    }

    PA_LOG(VERBOSE) << "Connected to Wi-Fi hotspot for device with ID "
                    << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                           device_id)
                    << ", but the connection to that device was canceled. "
                    << "Disconnecting.";

    // Disconnect from the Wi-Fi hotspot; otherwise, it is possible to be
    // connected to the Wi-Fi hotspot despite there being no active host. See
    // crbug.com/761171.
    wifi_hotspot_disconnector_->DisconnectFromWifiHotspot(
        wifi_network_guid, base::DoNothing(),
        base::Bind(&OnDisconnectFromWifiFailure, device_id));
    return;
  }

  if (wifi_network_guid.empty()) {
    // If the Wi-Fi network ID is empty, then the connection did not succeed.
    PA_LOG(ERROR) << "Failed to connect to the hotspot belonging to the device "
                  << "with ID "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         device_id)
                  << ".";

    SetConnectionFailed(
        NetworkConnectionHandler::kErrorConnectFailed,
        HostConnectionMetricsLogger::ConnectionToHostResult::
            CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_TIMEOUT);
    return;
  }

  SetConnectionSucceeded(device_id, wifi_network_guid);
}

HostConnectionMetricsLogger::ConnectionToHostResult
TetherConnectorImpl::GetConnectionToHostResultFromErrorCode(
    const std::string& device_id,
    ConnectTetheringOperation::HostResponseErrorCode error_code) {
  if (error_code ==
      ConnectTetheringOperation::HostResponseErrorCode::PROVISIONING_FAILED) {
    return HostConnectionMetricsLogger::ConnectionToHostResult::
        CONNECTION_RESULT_PROVISIONING_FAILED;
  }

  if (error_code ==
      ConnectTetheringOperation::HostResponseErrorCode::TETHERING_TIMEOUT) {
    const std::string tether_network_guid =
        device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
            device_id);
    if (host_scan_cache_->DoesHostRequireSetup(tether_network_guid)) {
      return HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_TETHERING_TIMED_OUT_FIRST_TIME_SETUP_WAS_REQUIRED;
    }

    return HostConnectionMetricsLogger::ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_TETHERING_TIMED_OUT_FIRST_TIME_SETUP_WAS_NOT_REQUIRED;
  }

  if (error_code ==
      ConnectTetheringOperation::HostResponseErrorCode::TETHERING_UNSUPPORTED) {
    return HostConnectionMetricsLogger::ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_TETHERING_UNSUPPORTED;
  }

  if (error_code ==
      ConnectTetheringOperation::HostResponseErrorCode::NO_CELL_DATA) {
    return HostConnectionMetricsLogger::ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_NO_CELL_DATA;
  }

  if (error_code == ConnectTetheringOperation::HostResponseErrorCode::
                        ENABLING_HOTSPOT_FAILED) {
    return HostConnectionMetricsLogger::ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_ENABLING_HOTSPOT_FAILED;
  }

  if (error_code == ConnectTetheringOperation::HostResponseErrorCode::
                        ENABLING_HOTSPOT_TIMEOUT) {
    return HostConnectionMetricsLogger::ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_ENABLING_HOTSPOT_TIMEOUT;
  }

  if (error_code ==
      ConnectTetheringOperation::HostResponseErrorCode::UNKNOWN_ERROR) {
    return HostConnectionMetricsLogger::ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_UNKNOWN_ERROR;
  }

  if (error_code == ConnectTetheringOperation::HostResponseErrorCode::
                        INVALID_HOTSPOT_CREDENTIALS) {
    return HostConnectionMetricsLogger::ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_INVALID_HOTSPOT_CREDENTIALS;
  }

  if (error_code ==
      ConnectTetheringOperation::HostResponseErrorCode::NO_RESPONSE) {
    if (did_send_successful_request_) {
      return HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_SUCCESSFUL_REQUEST_BUT_NO_RESPONSE;
    } else {
      return HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_NO_RESPONSE;
    }
  }

  return HostConnectionMetricsLogger::ConnectionToHostResult::
      CONNECTION_RESULT_FAILURE_UNRECOGNIZED_RESPONSE_ERROR;
}

}  // namespace tether

}  // namespace chromeos
