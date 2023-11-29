// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_CONNECTOR_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_CONNECTOR_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/tether/connect_tethering_operation.h"
#include "chromeos/ash/components/tether/host_connection_metrics_logger.h"
#include "chromeos/ash/components/tether/tether_connector.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace device_sync {
class DeviceSyncClient;
}

namespace secure_channel {
class SecureChannelClient;
}

class NetworkStateHandler;

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

  TetherConnectorImpl(const TetherConnectorImpl&) = delete;
  TetherConnectorImpl& operator=(const TetherConnectorImpl&) = delete;

  ~TetherConnectorImpl() override;

  void ConnectToNetwork(const std::string& tether_network_guid,
                        base::OnceClosure success_callback,
                        StringErrorCallback error_callback) override;

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

  void SetConnectionFailed(const std::string& error_name);
  void SetConnectionSucceeded(const std::string& device_id,
                              const std::string& wifi_network_guid);

  void OnTetherHostToConnectFetched(
      const std::string& device_id,
      absl::optional<multidevice::RemoteDeviceRef> tether_host_to_connect);
  void OnWifiConnection(const std::string& device_id,
                        const std::string& wifi_network_guid);
  void RecordConnectTetheringOperationResult(
      const std::string& device_id,
      ConnectTetheringOperation::HostResponseErrorCode error_code);

  raw_ptr<device_sync::DeviceSyncClient, ExperimentalAsh> device_sync_client_;
  raw_ptr<secure_channel::SecureChannelClient, ExperimentalAsh>
      secure_channel_client_;
  raw_ptr<NetworkConnectionHandler, ExperimentalAsh>
      network_connection_handler_;
  raw_ptr<NetworkStateHandler, ExperimentalAsh> network_state_handler_;
  raw_ptr<WifiHotspotConnector, ExperimentalAsh> wifi_hotspot_connector_;
  raw_ptr<ActiveHost, ExperimentalAsh> active_host_;
  raw_ptr<TetherHostFetcher, ExperimentalAsh> tether_host_fetcher_;
  raw_ptr<TetherHostResponseRecorder, ExperimentalAsh>
      tether_host_response_recorder_;
  raw_ptr<DeviceIdTetherNetworkGuidMap, ExperimentalAsh>
      device_id_tether_network_guid_map_;
  raw_ptr<HostScanCache, ExperimentalAsh> host_scan_cache_;
  raw_ptr<NotificationPresenter, ExperimentalAsh> notification_presenter_;
  raw_ptr<HostConnectionMetricsLogger, ExperimentalAsh>
      host_connection_metrics_logger_;
  raw_ptr<DisconnectTetheringRequestSender, ExperimentalAsh>
      disconnect_tethering_request_sender_;
  raw_ptr<WifiHotspotDisconnector, ExperimentalAsh> wifi_hotspot_disconnector_;

  bool did_send_successful_request_ = false;
  std::string device_id_pending_connection_;
  base::OnceClosure success_callback_;
  StringErrorCallback error_callback_;
  std::unique_ptr<ConnectTetheringOperation> connect_tethering_operation_;
  base::Time connect_to_host_start_time_;
  base::WeakPtrFactory<TetherConnectorImpl> weak_ptr_factory_{this};
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_CONNECTOR_IMPL_H_
