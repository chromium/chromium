// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/nearby_connection_manager.h"

#include <optional>

#include "base/containers/contains.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/secure_channel/authenticated_channel.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"

namespace ash::secure_channel {

NearbyConnectionManager::InitiatorConnectionAttemptMetadata::
    InitiatorConnectionAttemptMetadata(
        const BleDiscoveryStateChangeCallback&
            ble_discovery_state_change_callback,
        const NearbyConnectionStateChangeCallback&
            nearby_connection_change_callback,
        const SecureChannelStateChangeCallback& secure_channel_change_callback,
        ConnectionSuccessCallback success_callback,
        const FailureCallback& failure_callback)
    : ble_discovery_state_change_callback(ble_discovery_state_change_callback),
      nearby_connection_change_callback(nearby_connection_change_callback),
      secure_channel_change_callback(secure_channel_change_callback),
      success_callback(std::move(success_callback)),
      failure_callback(failure_callback) {}

NearbyConnectionManager::InitiatorConnectionAttemptMetadata::
    ~InitiatorConnectionAttemptMetadata() = default;

NearbyConnectionManager::NearbyConnectionManager() = default;

NearbyConnectionManager::~NearbyConnectionManager() = default;

void NearbyConnectionManager::SetNearbyConnector(
    mojo::PendingRemote<mojom::NearbyConnector> nearby_connector) {
  if (nearby_connector_)
    nearby_connector_.reset();

  nearby_connector_.Bind(std::move(nearby_connector));
}

bool NearbyConnectionManager::IsNearbyConnectorSet() const {
  return nearby_connector_.is_bound();
}

void NearbyConnectionManager::AttemptNearbyInitiatorConnection(
    const DeviceIdPair& device_id_pair,
    const BleDiscoveryStateChangeCallback& ble_discovery_state_change_callback,
    const NearbyConnectionStateChangeCallback&
        nearby_connection_change_callback,
    const SecureChannelStateChangeCallback& secure_channel_change_callback,
    ConnectionSuccessCallback success_callback,
    const FailureCallback& failure_callback) {
  if (base::Contains(id_pair_to_initiator_metadata_map_, device_id_pair)) {
    PA_LOG(ERROR) << "Tried to add Nearby initiator connection attempt, but "
                  << "one was already active. Device IDs: " << device_id_pair;
    NOTREACHED_IN_MIGRATION();
    return;
  }

  id_pair_to_initiator_metadata_map_.emplace(std::make_pair(
      device_id_pair,
      std::make_unique<InitiatorConnectionAttemptMetadata>(
          ble_discovery_state_change_callback,
          nearby_connection_change_callback, secure_channel_change_callback,
          std::move(success_callback), failure_callback)));
  remote_device_id_to_id_pair_map_[device_id_pair.remote_device_id()].insert(
      device_id_pair);

  PA_LOG(VERBOSE) << "Attempting Nearby connection for: " << device_id_pair;
  PerformAttemptNearbyInitiatorConnection(device_id_pair);
}

void NearbyConnectionManager::CancelNearbyInitiatorConnectionAttempt(
    const DeviceIdPair& device_id_pair) {
  RemoveRequestMetadata(device_id_pair);

  PA_LOG(VERBOSE) << "Canceling Nearby connection attempt for: "
                  << device_id_pair;
  PerformCancelNearbyInitiatorConnectionAttempt(device_id_pair);
}

mojom::NearbyConnector* NearbyConnectionManager::GetNearbyConnector() {
  if (!nearby_connector_.is_bound())
    return nullptr;
  return nearby_connector_.get();
}

const base::flat_set<DeviceIdPair>&
NearbyConnectionManager::GetDeviceIdPairsForRemoteDevice(
    const std::string& remote_device_id) const {
  return remote_device_id_to_id_pair_map_.find(remote_device_id)->second;
}

bool NearbyConnectionManager::DoesAttemptExist(
    const DeviceIdPair& device_id_pair) {
  return base::Contains(id_pair_to_initiator_metadata_map_, device_id_pair);
}

void NearbyConnectionManager::NotifyNearbyInitiatorFailure(
    const DeviceIdPair& device_id_pair,
    NearbyInitiatorFailureType failure_type) {
  PA_LOG(VERBOSE) << "Notifying client of Nearby initiator failure: "
                  << device_id_pair;
  GetInitiatorEntry(device_id_pair).failure_callback.Run(failure_type);
}

void NearbyConnectionManager::NotifyNearbyInitiatorConnectionSuccess(
    const DeviceIdPair& device_id_pair,
    std::unique_ptr<AuthenticatedChannel> authenticated_channel) {
  PA_LOG(VERBOSE) << "Notifying client of successful Neraby connection for: "
                  << device_id_pair;

  // Retrieve the success callback out of the map first, then remove the
  // associated metadata before invoking the callback.
  ConnectionSuccessCallback success_callback =
      std::move(GetInitiatorEntry(device_id_pair).success_callback);
  RemoveRequestMetadata(device_id_pair);
  std::move(success_callback).Run(std::move(authenticated_channel));
}

void NearbyConnectionManager::NotifyBleDiscoveryStateChanged(
    const DeviceIdPair& device_id_pair,
    mojom::DiscoveryResult discovery_result,
    std::optional<mojom::DiscoveryErrorCode> potential_error_code) {
  if (id_pair_to_initiator_metadata_map_.contains(device_id_pair)) {
    GetInitiatorEntry(device_id_pair)
        .ble_discovery_state_change_callback.Run(discovery_result,
                                                 potential_error_code);
  }
}

void NearbyConnectionManager::NotifyNearbyConnectionStateChanged(
    const DeviceIdPair& device_id_pair,
    mojom::NearbyConnectionStep step,
    mojom::NearbyConnectionStepResult result) {
  NearbyConnectionStateChangeCallback nearby_connection_state_changed_callback =
      GetInitiatorEntry(device_id_pair).nearby_connection_change_callback;
  std::move(nearby_connection_state_changed_callback).Run(step, result);
}

void NearbyConnectionManager::NotifySecureChannelAuthenticationStateChanged(
    const DeviceIdPair& device_id_pair,
    mojom::SecureChannelState secure_channel_authentication_state) {
  GetInitiatorEntry(device_id_pair)
      .secure_channel_change_callback.Run(secure_channel_authentication_state);
}

NearbyConnectionManager::InitiatorConnectionAttemptMetadata&
NearbyConnectionManager::GetInitiatorEntry(const DeviceIdPair& device_id_pair) {
  std::unique_ptr<InitiatorConnectionAttemptMetadata>& entry =
      id_pair_to_initiator_metadata_map_[device_id_pair];
  DCHECK(entry);
  return *entry;
}

void NearbyConnectionManager::RemoveRequestMetadata(
    const DeviceIdPair& device_id_pair) {
  auto metadata_it = id_pair_to_initiator_metadata_map_.find(device_id_pair);
  if (metadata_it == id_pair_to_initiator_metadata_map_.end()) {
    PA_LOG(ERROR) << "Tried to remove Nearby initiator metadata, but none "
                  << "existed. Device IDs: " << device_id_pair;
    NOTREACHED_IN_MIGRATION();
  } else {
    id_pair_to_initiator_metadata_map_.erase(metadata_it);
  }

  auto id_pair_it =
      remote_device_id_to_id_pair_map_.find(device_id_pair.remote_device_id());
  if (id_pair_it == remote_device_id_to_id_pair_map_.end()) {
    PA_LOG(ERROR) << "Tried to remove Nearby initiator attempt, but no attempt "
                  << "existed. Device IDs: " << device_id_pair;
    NOTREACHED_IN_MIGRATION();
  } else {
    id_pair_it->second.erase(device_id_pair);
  }
}

}  // namespace ash::secure_channel
