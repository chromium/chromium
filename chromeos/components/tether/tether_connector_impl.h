// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_TETHER_CONNECTOR_IMPL_H_
#define CHROMEOS_COMPONENTS_TETHER_TETHER_CONNECTOR_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chromeos/components/tether/connect_tethering_operation.h"
#include "chromeos/components/tether/host_connection_metrics_logger.h"
#include "chromeos/components/tether/tether_connector.h"
#include "chromeos/network/network_connection_handler.h"

namespace chromeos {

class NetworkStateHandler;

namespace device_sync {
class DeviceSyncClient;
}  // namespace device_sync

namespace secure_channel {
class SecureChannelClient;
}  // namespace secure_channel

namespace tether {

class ActiveHost;
class DeviceIdTetherNetworkGuidMap;
class DisconnectTetheringRequestSender;
class HostScanCache;
class NotificationPresenter;
class TetherHostFetcher;
class TetherHostResponseRecorder;
class WifiHotspotConnector;
class WifiHotspotDisconnector;

// Connects to a tether network. When the user initiates a connection via the
// UI, TetherConnectorImpl receives a callback from NetworkConnectionHandler and
// initiates a connection by starting a ConnectTetheringOperation. When a
// response has been received from the tether host, TetherConnectorImpl connects
// to the associated Wi-Fi network.
class TetherConnectorImpl : public TetherConnector,
                            public ConnectTetheringOperation::Observer {
 public:
  TetherConnectorImpl(
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
      WifiHotspotDisconnector* wifi_hotspot_disconnector);
  ~TetherConnectorImpl() override;

  void ConnectToNetwork(
      const std::string& tether_network_guid,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback) override;

  // Returns whether the connection attempt was successfully canceled.
  bool CancelConnectionAttempt(const std::string& tether_network_guid) override;

  // ConnectTetheringOperation::Observer:
  void OnConnectTetheringRequestSent(
      multidevice::RemoteDeviceRef remote_device) override;
  void OnSuccessfulConnectTetheringResponse(
      multidevice::RemoteDeviceRef remote_device,
      const std::string& ssid,
      const std::string& password) override;
  void OnConnectTetheringFailure(
      multidevice::RemoteDeviceRef remote_device,
      ConnectTetheringOperation::HostResponseErrorCode error_code) override;

 private:
  friend class TetherConnectorImplTest;

  void SetConnectionFailed(const std::string& error_name,
                           HostConnectionMetricsLogger::ConnectionToHostResult
                               connection_to_host_result);
  void SetConnectionSucceeded(const std::string& device_id,
                              const std::string& wifi_network_guid);

  void OnTetherHostToConnectFetched(
      const std::string& device_id,
      base::Optional<multidevice::RemoteDeviceRef> tether_host_to_connect);
  void OnWifiConnection(const std::string& device_id,
                        const std::string& wifi_network_guid);
  HostConnectionMetricsLogger::ConnectionToHostResult
  GetConnectionToHostResultFromErrorCode(
      const std::string& device_id,
      ConnectTetheringOperation::HostResponseErrorCode error_code);

  device_sync::DeviceSyncClient* device_sync_client_;
  secure_channel::SecureChannelClient* secure_channel_client_;
  NetworkConnectionHandler* network_connection_handler_;
  NetworkStateHandler* network_state_handler_;
  WifiHotspotConnector* wifi_hotspot_connector_;
  ActiveHost* active_host_;
  TetherHostFetcher* tether_host_fetcher_;
  TetherHostResponseRecorder* tether_host_response_recorder_;
  DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map_;
  HostScanCache* host_scan_cache_;
  NotificationPresenter* notification_presenter_;
  HostConnectionMetricsLogger* host_connection_metrics_logger_;
  DisconnectTetheringRequestSender* disconnect_tethering_request_sender_;
  WifiHotspotDisconnector* wifi_hotspot_disconnector_;

  bool did_send_successful_request_ = false;
  std::string device_id_pending_connection_;
  base::Closure success_callback_;
  network_handler::StringResultCallback error_callback_;
  std::unique_ptr<ConnectTetheringOperation> connect_tethering_operation_;
  base::Time connect_to_host_start_time_;
  base::WeakPtrFactory<TetherConnectorImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TetherConnectorImpl);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_TETHER_CONNECTOR_IMPL_H_
