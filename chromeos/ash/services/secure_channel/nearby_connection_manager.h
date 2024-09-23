// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_MANAGER_H_

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"
#include "chromeos/ash/services/secure_channel/nearby_initiator_failure_type.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::secure_channel {

class AuthenticatedChannel;

// Attempts connects to remote devices via the Nearby Connections library.
class NearbyConnectionManager {
 public:
  NearbyConnectionManager(const NearbyConnectionManager&) = delete;
  NearbyConnectionManager& operator=(const NearbyConnectionManager&) = delete;
  virtual ~NearbyConnectionManager();

  // Note: NearbyConnector must be set before connections can be requested.
  void SetNearbyConnector(
      mojo::PendingRemote<mojom::NearbyConnector> nearby_connector);
  bool IsNearbyConnectorSet() const;

  using BleDiscoveryStateChangeCallback =
      base::RepeatingCallback<void(mojom::DiscoveryResult,
                                   std::optional<mojom::DiscoveryErrorCode>)>;
  using NearbyConnectionStateChangeCallback =
      base::RepeatingCallback<void(mojom::NearbyConnectionStep,
                                   mojom::NearbyConnectionStepResult)>;
  using SecureChannelStateChangeCallback =
      base::RepeatingCallback<void(mojom::SecureChannelState)>;
  using ConnectionSuccessCallback =
      base::OnceCallback<void(std::unique_ptr<AuthenticatedChannel>)>;
  using FailureCallback =
      base::RepeatingCallback<void(NearbyInitiatorFailureType)>;

  // Attempts a connection, invoking the success/failure callback when the
  // attempt has finished.
  void AttemptNearbyInitiatorConnection(
      const DeviceIdPair& device_id_pair,
      const BleDiscoveryStateChangeCallback&
          ble_discovery_state_change_callback,
      const NearbyConnectionStateChangeCallback&
          nearby_connection_change_callback,
      const SecureChannelStateChangeCallback& secure_channel_change_callback,
      ConnectionSuccessCallback success_callback,
      const FailureCallback& failure_callback);

  // Cancels an active connection attempt; the success/failure callback for this
  // attempt will not be invoked.
  void CancelNearbyInitiatorConnectionAttempt(
      const DeviceIdPair& device_id_pair);

 protected:
  NearbyConnectionManager();

  virtual void PerformAttemptNearbyInitiatorConnection(
      const DeviceIdPair& device_id_pair) = 0;
  virtual void PerformCancelNearbyInitiatorConnectionAttempt(
      const DeviceIdPair& device_id_pair) = 0;

  mojom::NearbyConnector* GetNearbyConnector();

  const base::flat_set<DeviceIdPair>& GetDeviceIdPairsForRemoteDevice(
      const std::string& remote_device_id) const;
  bool DoesAttemptExist(const DeviceIdPair& device_id_pair);

  void NotifyNearbyInitiatorFailure(const DeviceIdPair& device_id_pair,
                                    NearbyInitiatorFailureType failure_type);
  void NotifyNearbyInitiatorConnectionSuccess(
      const DeviceIdPair& device_id_pair,
      std::unique_ptr<AuthenticatedChannel> authenticated_channel);

  void NotifyBleDiscoveryStateChanged(
      const DeviceIdPair& device_id_pair,
      mojom::DiscoveryResult discovery_result,
      std::optional<mojom::DiscoveryErrorCode> potential_error_code);
  void NotifyNearbyConnectionStateChanged(
      const DeviceIdPair& device_id_pair,
      mojom::NearbyConnectionStep step,
      mojom::NearbyConnectionStepResult result);
  void NotifySecureChannelAuthenticationStateChanged(
      const DeviceIdPair& device_id_pair,
      mojom::SecureChannelState secure_channel_authentication_state);

 private:
  struct InitiatorConnectionAttemptMetadata {
    InitiatorConnectionAttemptMetadata(
        const BleDiscoveryStateChangeCallback&
            ble_discovery_state_change_callback,
        const NearbyConnectionStateChangeCallback&
            nearby_connection_change_callback,
        const SecureChannelStateChangeCallback& secure_channel_change_callback,
        ConnectionSuccessCallback success_callback,
        const FailureCallback& failure_callback);
    ~InitiatorConnectionAttemptMetadata();

    BleDiscoveryStateChangeCallback ble_discovery_state_change_callback;
    NearbyConnectionStateChangeCallback nearby_connection_change_callback;
    SecureChannelStateChangeCallback secure_channel_change_callback;
    ConnectionSuccessCallback success_callback;
    FailureCallback failure_callback;
  };

  InitiatorConnectionAttemptMetadata& GetInitiatorEntry(
      const DeviceIdPair& device_id_pair);
  void RemoveRequestMetadata(const DeviceIdPair& device_id_pair);

  mojo::Remote<mojom::NearbyConnector> nearby_connector_;
  base::flat_map<std::string, base::flat_set<DeviceIdPair>>
      remote_device_id_to_id_pair_map_;
  base::flat_map<DeviceIdPair,
                 std::unique_ptr<InitiatorConnectionAttemptMetadata>>
      id_pair_to_initiator_metadata_map_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_MANAGER_H_
