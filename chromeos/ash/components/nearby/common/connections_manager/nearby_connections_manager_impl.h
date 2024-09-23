// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CONNECTIONS_MANAGER_NEARBY_CONNECTIONS_MANAGER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CONNECTIONS_MANAGER_NEARBY_CONNECTIONS_MANAGER_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connection_impl.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_file_handler.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_process_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

// Concrete NearbyConnectionsManager implementation.
class NearbyConnectionsManagerImpl
    : public NearbyConnectionsManager,
      public nearby::connections::mojom::EndpointDiscoveryListener,
      public nearby::connections::mojom::ConnectionLifecycleListener,
      public nearby::connections::mojom::PayloadListener,
      public nearby::connections::mojom::ConnectionListenerV3,
      public nearby::connections::mojom::PayloadListenerV3 {
 public:
  NearbyConnectionsManagerImpl(
      ash::nearby::NearbyProcessManager* process_manager,
      const std::string& service_id);
  ~NearbyConnectionsManagerImpl() override;
  NearbyConnectionsManagerImpl(const NearbyConnectionsManagerImpl&) = delete;
  NearbyConnectionsManagerImpl& operator=(const NearbyConnectionsManagerImpl&) =
      delete;

  // NearbyConnectionsManager:
  void Shutdown() override;
  void StartAdvertising(std::vector<uint8_t> endpoint_info,
                        IncomingConnectionListener* listener,
                        NearbyConnectionsManager::PowerLevel power_level,
                        DataUsage data_usage,
                        ConnectionsCallback callback) override;
  void StopAdvertising(ConnectionsCallback callback) override;
  void InjectBluetoothEndpoint(
      const std::string& service_id,
      const std::string& endpoint_id,
      const std::vector<uint8_t> endpoint_info,
      const std::vector<uint8_t> remote_bluetooth_mac_address,
      ConnectionsCallback callback) override;
  void StartDiscovery(DiscoveryListener* listener,
                      DataUsage data_usage,
                      ConnectionsCallback callback) override;
  void StopDiscovery() override;
  void Connect(std::vector<uint8_t> endpoint_info,
               const std::string& endpoint_id,
               std::optional<std::vector<uint8_t>> bluetooth_mac_address,
               DataUsage data_usage,
               NearbyConnectionCallback callback) override;
  void Disconnect(const std::string& endpoint_id) override;
  void Send(const std::string& endpoint_id,
            PayloadPtr payload,
            base::WeakPtr<PayloadStatusListener> listener) override;
  void RegisterPayloadStatusListener(
      int64_t payload_id,
      base::WeakPtr<PayloadStatusListener> listener) override;
  void RegisterPayloadPath(int64_t payload_id,
                           const base::FilePath& file_path,
                           ConnectionsCallback callback) override;
  Payload* GetIncomingPayload(int64_t payload_id) override;
  void Cancel(int64_t payload_id) override;
  void ClearIncomingPayloads() override;
  void ClearIncomingPayloadWithId(int64_t payload_id) override;
  std::optional<std::string> GetAuthenticationToken(
      const std::string& endpoint_id) override;
  std::optional<std::vector<uint8_t>> GetRawAuthenticationToken(
      const std::string& endpoint_id) override;
  void RegisterBandwidthUpgradeListener(
      base::WeakPtr<BandwidthUpgradeListener> listener) override;
  void UpgradeBandwidth(const std::string& endpoint_id) override;
  base::WeakPtr<NearbyConnectionsManager> GetWeakPtr() override;
  void ConnectV3(nearby::presence::PresenceDevice remote_presence_device,
                 DataUsage data_usage,
                 NearbyConnectionCallback callback) override;
  void DisconnectV3(
      nearby::presence::PresenceDevice remote_presence_device) override;

 protected:
  raw_ptr<nearby::connections::mojom::NearbyConnections> GetNearbyConnections();

  raw_ptr<ash::nearby::NearbyProcessManager> process_manager_;
  const std::string service_id_;

 private:
  using AdvertisingOptions = nearby::connections::mojom::AdvertisingOptions;
  using ConnectionInfoPtr = nearby::connections::mojom::ConnectionInfoPtr;
  using ConnectionOptions = nearby::connections::mojom::ConnectionOptions;
  using ConnectionLifecycleListener =
      nearby::connections::mojom::ConnectionLifecycleListener;
  using DiscoveredEndpointInfoPtr =
      nearby::connections::mojom::DiscoveredEndpointInfoPtr;
  using DiscoveryOptions = nearby::connections::mojom::DiscoveryOptions;
  using EndpointDiscoveryListener =
      nearby::connections::mojom::EndpointDiscoveryListener;
  using MediumSelection = nearby::connections::mojom::MediumSelection;
  using PayloadListener = nearby::connections::mojom::PayloadListener;
  using PayloadTransferUpdate =
      nearby::connections::mojom::PayloadTransferUpdate;
  using PayloadStatus = nearby::connections::mojom::PayloadStatus;
  using PayloadTransferUpdatePtr =
      nearby::connections::mojom::PayloadTransferUpdatePtr;
  using ConnectionListenerV3 = nearby::connections::mojom::ConnectionListenerV3;
  using PresenceDevicePtr = ash::nearby::presence::mojom::PresenceDevicePtr;
  using InitialConnectionInfoV3Ptr =
      nearby::connections::mojom::InitialConnectionInfoV3Ptr;
  using PayloadListenerV3 = nearby::connections::mojom::PayloadListenerV3;
  using BandwidthInfoPtr = nearby::connections::mojom::BandwidthInfoPtr;
  using Status = nearby::connections::mojom::Status;
  using Medium = nearby::connections::mojom::Medium;

  FRIEND_TEST_ALL_PREFIXES(NearbyConnectionsManagerImplTest,
                           DiscoveryProcessStopped);

  // EndpointDiscoveryListener:
  void OnEndpointFound(const std::string& endpoint_id,
                       DiscoveredEndpointInfoPtr info) override;
  void OnEndpointLost(const std::string& endpoint_id) override;

  // ConnectionLifecycleListener:
  void OnConnectionInitiated(const std::string& endpoint_id,
                             ConnectionInfoPtr info) override;
  void OnConnectionAccepted(const std::string& endpoint_id) override;
  void OnConnectionRejected(const std::string& endpoint_id,
                            Status status) override;
  void OnDisconnected(const std::string& endpoint_id) override;
  void OnBandwidthChanged(const std::string& endpoint_id,
                          Medium medium) override;

  // PayloadListener:
  void OnPayloadReceived(const std::string& endpoint_id,
                         PayloadPtr payload) override;
  void OnPayloadTransferUpdate(const std::string& endpoint_id,
                               PayloadTransferUpdatePtr update) override;

  // ConnectionListenerV3:
  void OnConnectionInitiatedV3(const std::string& endpoint_id,
                               InitialConnectionInfoV3Ptr info) override;
  void OnConnectionResultV3(const std::string& endpoint_id,
                            Status status) override;
  void OnDisconnectedV3(const std::string& endpoint_id) override;
  void OnBandwidthChangedV3(const std::string& endpoint_id,
                            BandwidthInfoPtr bandwidth_info) override;

  // PayloadListenerV3:
  void OnPayloadReceivedV3(const std::string& endpoint_id,
                           PayloadPtr payload) override;
  void OnPayloadTransferUpdateV3(const std::string& endpoint_id,
                                 PayloadTransferUpdatePtr update) override;

  void OnConnectionTimedOut(const std::string& endpoint_id);
  void OnConnectionTimedOutV3(const std::string& endpoint_id);
  void OnConnectionRequested(const std::string& endpoint_id,
                             ConnectionsStatus status);
  void OnConnectionRequestedV3(
      nearby::presence::PresenceDevice remote_presence_device,
      ConnectionsStatus status);
  void OnNearbyProcessStopped(
      ash::nearby::NearbyProcessManager::NearbyProcessShutdownReason
          shutdown_reason);
  void Reset();

  void OnFileCreated(int64_t payload_id,
                     ConnectionsCallback callback,
                     NearbyFileHandler::CreateFileResult result);

  // For metrics.
  std::optional<Medium> GetUpgradedMedium(const std::string& endpoint_id) const;

  std::unique_ptr<ash::nearby::NearbyProcessManager::NearbyProcessReference>
      process_reference_;
  NearbyFileHandler file_handler_;
  raw_ptr<IncomingConnectionListener> incoming_connection_listener_ = nullptr;
  raw_ptr<DiscoveryListener> discovery_listener_ = nullptr;
  base::WeakPtr<BandwidthUpgradeListener> bandwidth_upgrade_listener_;
  base::flat_set<std::string> discovered_endpoints_;
  // A map of endpoint_id to NearbyConnectionCallback.
  base::flat_map<std::string, NearbyConnectionCallback>
      pending_outgoing_connections_;
  // A map of endpoint_id to ConnectionInfoPtr.
  base::flat_map<std::string, ConnectionInfoPtr> connection_info_map_;
  // A map of endpoint_id to NearbyConnection.
  base::flat_map<std::string, std::unique_ptr<NearbyConnectionImpl>>
      connections_;
  // A map of endpoint_id to NearbyConnection for V3 connections.
  base::flat_map<std::string, std::unique_ptr<NearbyConnectionImpl>>
      connections_v3_;
  // A map of endpoint_id to `PresenceDevice` to pass back to any listening
  // clients.
  base::flat_map<std::string, std::unique_ptr<nearby::presence::PresenceDevice>>
      endpoint_id_to_presence_device_map_;
  // A map of endpoint_id to timers that timeout a connection request.
  base::flat_map<std::string, std::unique_ptr<base::OneShotTimer>>
      connect_timeout_timers_;
  // A map of endpoint_id to timers that timeout a V3 connection request.
  base::flat_map<std::string, std::unique_ptr<base::OneShotTimer>>
      connect_timeout_timers_v3_;
  // A map of payload_id to PayloadStatusListener weak pointer.
  base::flat_map<int64_t, base::WeakPtr<PayloadStatusListener>>
      payload_status_listeners_;
  // A map of payload_id to PayloadPtr.
  base::flat_map<int64_t, PayloadPtr> incoming_payloads_;

  // For metrics. A set of endpoint_ids for which we have requested a bandwidth
  // upgrade.
  base::flat_set<std::string> requested_bwu_endpoint_ids_;
  // For metrics. A set of endpoint_ids for which we have received the first
  // OnBandwidthChanged event.
  base::flat_set<std::string> on_bandwidth_changed_endpoint_ids_;
  // For metrics. A map of endpoint_id to current upgraded medium.
  base::flat_map<std::string, Medium> current_upgraded_mediums_;
  // For metrics. A set of endpoint_ids for which we have received the first
  // OnBandwidthChanged V3 event.
  base::flat_set<std::string> on_bandwidth_changed_endpoint_ids_v3_;
  // For metrics. A map of endpoint_id to current upgraded medium for V3
  // connections.
  base::flat_map<std::string, Medium> current_upgraded_mediums_v3_;
  // For metrics. A map of endpoint_id to `base::TimeTicks` representing the
  // start time when `ConnectV3()` is called.
  base::flat_map<std::string, base::TimeTicks>
      endpoint_id_to_connect_v3_start_time_;

  mojo::Receiver<EndpointDiscoveryListener> endpoint_discovery_listener_{this};
  mojo::ReceiverSet<ConnectionLifecycleListener>
      connection_lifecycle_listeners_;
  mojo::ReceiverSet<PayloadListener> payload_listeners_;
  mojo::ReceiverSet<ConnectionListenerV3> connection_listener_v3s_;
  mojo::ReceiverSet<PayloadListenerV3> payload_listener_v3s_;

  base::WeakPtrFactory<NearbyConnectionsManagerImpl> weak_ptr_factory_{this};
};

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CONNECTIONS_MANAGER_NEARBY_CONNECTIONS_MANAGER_IMPL_H_
