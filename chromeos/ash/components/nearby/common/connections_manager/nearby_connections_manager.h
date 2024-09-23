// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CONNECTIONS_MANAGER_NEARBY_CONNECTIONS_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CONNECTIONS_MANAGER_NEARBY_CONNECTIONS_MANAGER_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connection.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "third_party/nearby/src/presence/presence_device.h"

// A wrapper around the Nearby Connections mojo API.
class NearbyConnectionsManager {
 public:
  // Represents the advertising bluetooth power for Nearby Connections.
  enum class PowerLevel {
    kUnknown = 0,
    kLowPower = 1,
    kMediumPower = 2,
    kHighPower = 3,
    kMaxValue = kHighPower
  };

  using PresenceDevice = nearby::presence::PresenceDevice;
  using Payload = nearby::connections::mojom::Payload;
  using PayloadPtr = nearby::connections::mojom::PayloadPtr;
  using ConnectionsStatus = nearby::connections::mojom::Status;
  using ConnectionsCallback =
      base::OnceCallback<void(ConnectionsStatus status)>;
  using NearbyConnectionCallback = base::OnceCallback<void(NearbyConnection*)>;
  using DataUsage = nearby_share::mojom::DataUsage;

  // A callback for handling incoming connections while advertising.
  class IncomingConnectionListener {
   public:
    virtual ~IncomingConnectionListener() = default;

    // Called when the remote device initiates a connection, but has not yet
    // accepted the connection.
    // |endpoint_info| is returned from remote devices and should be parsed in
    // utilitiy process.
    virtual void OnIncomingConnectionInitiated(
        const std::string& endpoint_id,
        const std::vector<uint8_t>& endpoint_info) = 0;

    // Called after the remote device has initiated and accepted the connection.
    // |endpoint_info| is returned from remote devices and should be parsed in
    // utilitiy process.
    virtual void OnIncomingConnectionAccepted(
        const std::string& endpoint_id,
        const std::vector<uint8_t>& endpoint_info,
        NearbyConnection* connection) = 0;
  };

  // A callback for handling discovered devices while discovering.
  class DiscoveryListener {
   public:
    virtual ~DiscoveryListener() = default;

    // |endpoint_info| is returned from remote devices and should be parsed in
    // utilitiy process.
    virtual void OnEndpointDiscovered(
        const std::string& endpoint_id,
        const std::vector<uint8_t>& endpoint_info) = 0;

    virtual void OnEndpointLost(const std::string& endpoint_id) = 0;
  };

  // A callback for tracking the status of a payload (both incoming and
  // outgoing).
  class PayloadStatusListener {
   public:
    using Medium = nearby::connections::mojom::Medium;
    using PayloadTransferUpdatePtr =
        nearby::connections::mojom::PayloadTransferUpdatePtr;

    PayloadStatusListener();
    virtual ~PayloadStatusListener();

    base::WeakPtr<PayloadStatusListener> GetWeakPtr();

    // Note: |upgraded_medium| is passed in for use in metrics, and it is
    // std::nullopt if the bandwidth has not upgraded yet or if the upgrade
    // status is not known.
    virtual void OnStatusUpdate(PayloadTransferUpdatePtr update,
                                std::optional<Medium> upgraded_medium) = 0;

    base::WeakPtrFactory<PayloadStatusListener> weak_ptr_factory_{this};
  };

  // An optional callback to be notified when bandwidth upgrades complete
  // successfully.
  class BandwidthUpgradeListener {
   public:
    using Medium = nearby::connections::mojom::Medium;

    virtual ~BandwidthUpgradeListener() = default;

    // Called for the first time the medium is set for the associated
    // `endpoint_id`.
    virtual void OnInitialMedium(const std::string& endpoint_id,
                                 const Medium medium) = 0;

    // Called for each successful bandwidth upgrade for the associated
    // `endpoint_id`.
    virtual void OnBandwidthUpgrade(const std::string& endpoint_id,
                                    const Medium medium) = 0;

    // Called for each successful V3 bandwidth upgrade for the associated
    // `PresenceDevice`.
    virtual void OnBandwidthUpgradeV3(PresenceDevice remote_device,
                                      const Medium medium) = 0;
  };

  // Converts the status to a logging-friendly string.
  static std::string ConnectionsStatusToString(ConnectionsStatus status);

  virtual ~NearbyConnectionsManager() = default;

  // Disconnects from all endpoints and shut down Nearby Connections.
  // As a side effect of this call, both StopAdvertising and StopDiscovery may
  // be invoked if Nearby Connections is advertising or discovering.
  virtual void Shutdown() = 0;

  // Starts advertising through Nearby Connections. Caller is expected to ensure
  // |listener| remains valid until StopAdvertising is called.
  virtual void StartAdvertising(
      std::vector<uint8_t> endpoint_info,
      IncomingConnectionListener* listener,
      NearbyConnectionsManager::PowerLevel power_level,
      DataUsage data_usage,
      ConnectionsCallback callback) = 0;

  // Stops advertising through Nearby Connections.
  virtual void StopAdvertising(ConnectionsCallback callback) = 0;

  // Starts discovery through Nearby Connections. Caller is expected to ensure
  // |listener| remains valid until StopDiscovery is called.
  virtual void StartDiscovery(DiscoveryListener* listener,
                              DataUsage data_usage,
                              ConnectionsCallback callback) = 0;

  // Stops discovery through Nearby Connections.
  virtual void StopDiscovery() = 0;

  // Inject a bluetooth endpoint into a Nearby Connections discovery session
  // for the provided `service_id`.
  virtual void InjectBluetoothEndpoint(
      const std::string& service_id,
      const std::string& endpoint_id,
      const std::vector<uint8_t> endpoint_info,
      const std::vector<uint8_t> remote_bluetooth_mac_address,
      ConnectionsCallback callback) = 0;

  // Connects to remote |endpoint_id| through Nearby Connections.
  virtual void Connect(
      std::vector<uint8_t> endpoint_info,
      const std::string& endpoint_id,
      std::optional<std::vector<uint8_t>> bluetooth_mac_address,
      DataUsage data_usage,
      NearbyConnectionCallback callback) = 0;

  // Disconnects from remote |endpoint_id| through Nearby Connections.
  virtual void Disconnect(const std::string& endpoint_id) = 0;

  // Sends |payload| through Nearby Connections.
  virtual void Send(const std::string& endpoint_id,
                    PayloadPtr payload,
                    base::WeakPtr<PayloadStatusListener> listener) = 0;

  // Register a |listener| with |payload_id|.
  virtual void RegisterPayloadStatusListener(
      int64_t payload_id,
      base::WeakPtr<PayloadStatusListener> listener) = 0;

  // Register a |file_path| for receiving incoming payload with |payload_id|.
  virtual void RegisterPayloadPath(int64_t payload_id,
                                   const base::FilePath& file_path,
                                   ConnectionsCallback callback) = 0;

  // Gets the payload associated with |payload_id| if available.
  virtual Payload* GetIncomingPayload(int64_t payload_id) = 0;

  // Cancels a Payload currently in-flight to or from remote endpoints.
  virtual void Cancel(int64_t payload_id) = 0;

  // Clears all incoming payloads.
  virtual void ClearIncomingPayloads() = 0;

  // Clears a specific incoming payload with the given `payload_id`.
  virtual void ClearIncomingPayloadWithId(int64_t payload_id) = 0;

  // Gets the user-readable authentication token for the |endpoint_id|.
  virtual std::optional<std::string> GetAuthenticationToken(
      const std::string& endpoint_id) = 0;

  // Gets the raw authentication token for the |endpoint_id|.
  virtual std::optional<std::vector<uint8_t>> GetRawAuthenticationToken(
      const std::string& endpoint_id) = 0;

  // Register a |listener| with for bandwidth upgrades.
  virtual void RegisterBandwidthUpgradeListener(
      base::WeakPtr<BandwidthUpgradeListener> listener) = 0;

  // Initiates bandwidth upgrade for |endpoint_id|.
  virtual void UpgradeBandwidth(const std::string& endpoint_id) = 0;

  // Connects to a |remote_presence_device| through Nearby Connections.
  // TODO(b/306188252): Once ConnectionsDevice is implemented, change to take in
  // the NearbyDevice base class instead of PresenceDevice.
  virtual void ConnectV3(PresenceDevice remote_presence_device,
                         DataUsage data_usage,
                         NearbyConnectionCallback callback) = 0;

  // Disconnects from a |remote_presence_device| through Nearby Connections.
  // TODO(b/306188252): Once ConnectionsDevice is implemented, change to take in
  // the NearbyDevice base class instead of PresenceDevice.
  virtual void DisconnectV3(PresenceDevice remote_presence_device) = 0;

  virtual base::WeakPtr<NearbyConnectionsManager> GetWeakPtr() = 0;
};

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CONNECTIONS_MANAGER_NEARBY_CONNECTIONS_MANAGER_H_
