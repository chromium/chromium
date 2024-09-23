// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/ble_connection_manager.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/secure_channel/authenticated_channel.h"

namespace ash::secure_channel {

BleConnectionManager::InitiatorConnectionAttemptMetadata::
    InitiatorConnectionAttemptMetadata(
        ConnectionPriority connection_priority,
        ConnectionSuccessCallback success_callback,
        const BleInitiatorFailureCallback& failure_callback)
    : connection_priority(connection_priority),
      success_callback(std::move(success_callback)),
      failure_callback(failure_callback) {}

BleConnectionManager::InitiatorConnectionAttemptMetadata::
    ~InitiatorConnectionAttemptMetadata() = default;

BleConnectionManager::ListenerConnectionAttemptMetadata::
    ListenerConnectionAttemptMetadata(
        ConnectionPriority connection_priority,
        ConnectionSuccessCallback success_callback,
        const BleListenerFailureCallback& failure_callback)
    : connection_priority(connection_priority),
      success_callback(std::move(success_callback)),
      failure_callback(failure_callback) {}

BleConnectionManager::ListenerConnectionAttemptMetadata::
    ~ListenerConnectionAttemptMetadata() = default;

BleConnectionManager::BleConnectionManager() = default;

BleConnectionManager::~BleConnectionManager() = default;

void BleConnectionManager::AttemptBleInitiatorConnection(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority,
    ConnectionSuccessCallback success_callback,
    const BleInitiatorFailureCallback& failure_callback) {
  if (base::Contains(id_pair_to_initiator_metadata_map_, device_id_pair)) {
    PA_LOG(ERROR) << "BleConnectionManager::AttemptBleInitiatorConnection(): "
                  << "Tried to add BLE initiator connection attempt, but one "
                  << "was already active. Device IDs: " << device_id_pair
                  << ", Priority: " << connection_priority;
    NOTREACHED_IN_MIGRATION();
  }

  ConnectionAttemptDetails details(device_id_pair,
                                   ConnectionMedium::kBluetoothLowEnergy,
                                   ConnectionRole::kInitiatorRole);
  remote_device_id_to_details_map_[device_id_pair.remote_device_id()].insert(
      details);
  id_pair_to_initiator_metadata_map_.emplace(std::make_pair(
      device_id_pair,
      std::make_unique<InitiatorConnectionAttemptMetadata>(
          connection_priority, std::move(success_callback), failure_callback)));

  PA_LOG(VERBOSE) << "BleConnectionManager::AttemptBleInitiatorConnection(): "
                  << "Attempting connection; details: " << details;
  PerformAttemptBleInitiatorConnection(device_id_pair, connection_priority);
}

void BleConnectionManager::UpdateBleInitiatorConnectionPriority(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority) {
  auto& initiator_entry = GetInitiatorEntry(device_id_pair);
  if (initiator_entry.connection_priority == connection_priority) {
    PA_LOG(WARNING) << "BleConnectionManager::"
                    << "UpdateBleInitiatorConnectionPriority(): "
                    << "Tried to update BLE initiator connection attempt, but "
                    << "the provided priority was the same as the previous "
                    << "priority. Device IDs: " << device_id_pair
                    << ", Priority: " << connection_priority;
    return;
  }

  initiator_entry.connection_priority = connection_priority;

  PA_LOG(VERBOSE)
      << "BleConnectionManager::"
      << "UpdateBleInitiatorConnectionPriority(): Updating connection "
      << "priority; ID pair: " << device_id_pair
      << ", Priority: " << connection_priority;
  PerformUpdateBleInitiatorConnectionPriority(device_id_pair,
                                              connection_priority);
}

void BleConnectionManager::CancelBleInitiatorConnectionAttempt(
    const DeviceIdPair& device_id_pair) {
  RemoveRequestMetadata(device_id_pair, ConnectionRole::kInitiatorRole);

  PA_LOG(VERBOSE)
      << "BleConnectionManager::"
      << "CancelBleInitiatorConnectionAttempt(): Canceling connection "
      << "attempt; ID pair: " << device_id_pair;
  PerformCancelBleInitiatorConnectionAttempt(device_id_pair);
}

void BleConnectionManager::AttemptBleListenerConnection(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority,
    ConnectionSuccessCallback success_callback,
    const BleListenerFailureCallback& failure_callback) {
  if (base::Contains(id_pair_to_listener_metadata_map_, device_id_pair)) {
    PA_LOG(ERROR) << "BleConnectionManager::AttemptBleListenerConnection(): "
                  << "Tried to add BLE listener connection attempt, but one "
                  << "was already active. Device IDs: " << device_id_pair
                  << ", Priority: " << connection_priority;
    NOTREACHED_IN_MIGRATION();
  }

  ConnectionAttemptDetails details(device_id_pair,
                                   ConnectionMedium::kBluetoothLowEnergy,
                                   ConnectionRole::kListenerRole);
  remote_device_id_to_details_map_[device_id_pair.remote_device_id()].insert(
      details);
  id_pair_to_listener_metadata_map_.emplace(std::make_pair(
      device_id_pair,
      std::make_unique<ListenerConnectionAttemptMetadata>(
          connection_priority, std::move(success_callback), failure_callback)));

  PA_LOG(VERBOSE) << "BleConnectionManager::AttemptBleListenerConnection(): "
                  << "Attempting connection; details: " << details;
  PerformAttemptBleListenerConnection(device_id_pair, connection_priority);
}

void BleConnectionManager::UpdateBleListenerConnectionPriority(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority) {
  auto& listener_entry = GetListenerEntry(device_id_pair);
  if (listener_entry.connection_priority == connection_priority) {
    PA_LOG(WARNING) << "BleConnectionManager::"
                    << "UpdateBleListenerConnectionPriority(): "
                    << "Tried to update BLE listener connection attempt, but "
                    << "the provided priority was the same as the previous "
                    << "priority. Device IDs: " << device_id_pair
                    << ", Priority: " << connection_priority;
    return;
  }

  listener_entry.connection_priority = connection_priority;

  PA_LOG(VERBOSE)
      << "BleConnectionManager::"
      << "UpdateBleListenerConnectionPriority(): Updating connection "
      << "priority; ID pair: " << device_id_pair << ", Priority"
      << connection_priority;
  PerformUpdateBleListenerConnectionPriority(device_id_pair,
                                             connection_priority);
}

void BleConnectionManager::CancelBleListenerConnectionAttempt(
    const DeviceIdPair& device_id_pair) {
  RemoveRequestMetadata(device_id_pair, ConnectionRole::kListenerRole);

  PA_LOG(VERBOSE)
      << "BleConnectionManager::"
      << "CancelBleListenerConnectionAttempt(): Canceling connection "
      << "attempt; ID pair: " << device_id_pair;
  PerformCancelBleListenerConnectionAttempt(device_id_pair);
}

ConnectionPriority BleConnectionManager::GetPriorityForAttempt(
    const DeviceIdPair& device_id_pair,
    ConnectionRole connection_role) {
  switch (connection_role) {
    case ConnectionRole::kInitiatorRole:
      return GetInitiatorEntry(device_id_pair).connection_priority;
    case ConnectionRole::kListenerRole:
      return GetListenerEntry(device_id_pair).connection_priority;
  }
}

const base::flat_set<ConnectionAttemptDetails>&
BleConnectionManager::GetDetailsForRemoteDevice(
    const std::string& remote_device_id) {
  if (!base::Contains(remote_device_id_to_details_map_, remote_device_id)) {
    PA_LOG(ERROR) << "BleConnectionManager::GetDetailsForRemoteDevice(): Tried "
                  << "to get details for a remote device, but no device with "
                  << "the provided ID existed. ID: "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         remote_device_id);
    NOTREACHED_IN_MIGRATION();
  }

  return remote_device_id_to_details_map_[remote_device_id];
}

bool BleConnectionManager::DoesAttemptExist(const DeviceIdPair& device_id_pair,
                                            ConnectionRole connection_role) {
  switch (connection_role) {
    case ConnectionRole::kInitiatorRole:
      return base::Contains(id_pair_to_initiator_metadata_map_, device_id_pair);
    case ConnectionRole::kListenerRole:
      return base::Contains(id_pair_to_listener_metadata_map_, device_id_pair);
  }
}

void BleConnectionManager::NotifyBleInitiatorFailure(
    const DeviceIdPair& device_id_pair,
    BleInitiatorFailureType failure_type) {
  PA_LOG(VERBOSE) << "BleConnectionManager::NotifyBleInitiatorFailure(): "
                  << "Notifying client of failure. ID pair: " << device_id_pair
                  << ", Failure type: " << failure_type;
  GetInitiatorEntry(device_id_pair).failure_callback.Run(failure_type);
}

void BleConnectionManager::NotifyBleListenerFailure(
    const DeviceIdPair& device_id_pair,
    BleListenerFailureType failure_type) {
  PA_LOG(VERBOSE) << "BleConnectionManager::NotifyBleListenerFailure(): "
                  << "Notifying client of failure. ID pair: " << device_id_pair
                  << ", Failure type: " << failure_type;
  GetListenerEntry(device_id_pair).failure_callback.Run(failure_type);
}

void BleConnectionManager::NotifyConnectionSuccess(
    const DeviceIdPair& device_id_pair,
    ConnectionRole connection_role,
    std::unique_ptr<AuthenticatedChannel> authenticated_channel) {
  PA_LOG(VERBOSE) << "BleConnectionManager::NotifyConnectionSuccess(): "
                  << "Notifying client of successful connection. ID pair: "
                  << device_id_pair << ", Role: " << connection_role;

  // For each case, grab the success callback out of the map first, then remove
  // the associated metadata before invoking the callback.
  switch (connection_role) {
    case ConnectionRole::kInitiatorRole: {
      ConnectionSuccessCallback success_callback =
          std::move(GetInitiatorEntry(device_id_pair).success_callback);
      RemoveRequestMetadata(device_id_pair, ConnectionRole::kInitiatorRole);
      std::move(success_callback).Run(std::move(authenticated_channel));
      break;
    }

    case ConnectionRole::kListenerRole: {
      ConnectionSuccessCallback success_callback =
          std::move(GetListenerEntry(device_id_pair).success_callback);
      RemoveRequestMetadata(device_id_pair, ConnectionRole::kListenerRole);
      std::move(success_callback).Run(std::move(authenticated_channel));
      break;
    }
  }
}

BleConnectionManager::InitiatorConnectionAttemptMetadata&
BleConnectionManager::GetInitiatorEntry(const DeviceIdPair& device_id_pair) {
  if (!base::Contains(id_pair_to_initiator_metadata_map_, device_id_pair)) {
    PA_LOG(ERROR) << "BleConnectionManager::GetInitiatorEntry(): Tried to get "
                  << "map entry, but it did not exist. Device IDs: "
                  << device_id_pair;
    NOTREACHED_IN_MIGRATION();
  }

  std::unique_ptr<InitiatorConnectionAttemptMetadata>& entry =
      id_pair_to_initiator_metadata_map_[device_id_pair];
  DCHECK(entry);
  return *entry;
}

BleConnectionManager::ListenerConnectionAttemptMetadata&
BleConnectionManager::GetListenerEntry(const DeviceIdPair& device_id_pair) {
  if (!base::Contains(id_pair_to_listener_metadata_map_, device_id_pair)) {
    PA_LOG(ERROR) << "BleConnectionManager::GetListenerEntry(): Tried to get "
                  << "map entry, but it did not exist. Device IDs: "
                  << device_id_pair;
    NOTREACHED_IN_MIGRATION();
  }

  std::unique_ptr<ListenerConnectionAttemptMetadata>& entry =
      id_pair_to_listener_metadata_map_[device_id_pair];
  DCHECK(entry);
  return *entry;
}

void BleConnectionManager::RemoveRequestMetadata(
    const DeviceIdPair& device_id_pair,
    ConnectionRole connection_role) {
  switch (connection_role) {
    case ConnectionRole::kInitiatorRole:
      if (!base::Contains(id_pair_to_initiator_metadata_map_, device_id_pair)) {
        PA_LOG(ERROR) << "BleConnectionManager::RemoveRequestMetadata(): Tried "
                      << "to remove BLE initiator attempt, but no attempt "
                      << "existed. Device IDs: " << device_id_pair;
        NOTREACHED_IN_MIGRATION();
      }

      id_pair_to_initiator_metadata_map_.erase(device_id_pair);
      break;

    case ConnectionRole::kListenerRole:
      if (!base::Contains(id_pair_to_listener_metadata_map_, device_id_pair)) {
        PA_LOG(ERROR) << "BleConnectionManager::RemoveRequestMetadata(): Tried "
                      << "to remove BLE listener attempt, but no attempt "
                      << "existed. Device IDs: " << device_id_pair;
        NOTREACHED_IN_MIGRATION();
      }

      id_pair_to_listener_metadata_map_.erase(device_id_pair);
      break;
  }

  ConnectionAttemptDetails details(
      device_id_pair, ConnectionMedium::kBluetoothLowEnergy, connection_role);

  size_t num_removed =
      remote_device_id_to_details_map_[device_id_pair.remote_device_id()].erase(
          details);
  if (num_removed != 1u) {
    PA_LOG(ERROR) << "BleConnectionManager::RemoveRequestMetadata(): Tried "
                  << "to remove connection attempt, but no remote device ID "
                  << "entry existed. Device IDs: " << device_id_pair;
    NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace ash::secure_channel
