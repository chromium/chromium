// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/pending_connection_manager_impl.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/services/secure_channel/authenticated_channel.h"
#include "chromeos/ash/services/secure_channel/ble_initiator_connection_attempt.h"
#include "chromeos/ash/services/secure_channel/ble_listener_connection_attempt.h"
#include "chromeos/ash/services/secure_channel/nearby_initiator_connection_attempt.h"
#include "chromeos/ash/services/secure_channel/pending_ble_initiator_connection_request.h"
#include "chromeos/ash/services/secure_channel/pending_ble_listener_connection_request.h"
#include "chromeos/ash/services/secure_channel/pending_nearby_initiator_connection_request.h"

namespace ash::secure_channel {

// static
PendingConnectionManagerImpl::Factory*
    PendingConnectionManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<PendingConnectionManager>
PendingConnectionManagerImpl::Factory::Create(
    Delegate* delegate,
    BleConnectionManager* ble_connection_manager,
    NearbyConnectionManager* nearby_connection_manager,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  if (test_factory_) {
    return test_factory_->CreateInstance(delegate, ble_connection_manager,
                                         nearby_connection_manager,
                                         bluetooth_adapter);
  }

  return base::WrapUnique(new PendingConnectionManagerImpl(
      delegate, ble_connection_manager, nearby_connection_manager,
      bluetooth_adapter));
}

// static
void PendingConnectionManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

PendingConnectionManagerImpl::Factory::~Factory() = default;

PendingConnectionManagerImpl::PendingConnectionManagerImpl(
    Delegate* delegate,
    BleConnectionManager* ble_connection_manager,
    NearbyConnectionManager* nearby_connection_manager,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
    : PendingConnectionManager(delegate),
      ble_connection_manager_(ble_connection_manager),
      nearby_connection_manager_(nearby_connection_manager),
      bluetooth_adapter_(bluetooth_adapter) {}

PendingConnectionManagerImpl::~PendingConnectionManagerImpl() = default;

void PendingConnectionManagerImpl::HandleConnectionRequest(
    const ConnectionAttemptDetails& connection_attempt_details,
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    ConnectionPriority connection_priority) {
  // If the client has canceled the request, it does not need to be processed.
  if (!client_connection_parameters->IsClientWaitingForResponse()) {
    PA_LOG(VERBOSE)
        << "PendingConnectionManagerImpl::HandleConnectionRequest(): "
        << "Request was canceled by the client before being passed to "
        << "PendingConnectionManager; ignoring. Details: "
        << connection_attempt_details
        << ", Parameters: " << *client_connection_parameters
        << ", Priority: " << connection_priority;
    return;
  }

  // Insert the entry into the ConnectionDetails to ConnectionAttemptDetails
  // map.
  ConnectionDetails connection_details =
      connection_attempt_details.GetAssociatedConnectionDetails();
  details_to_attempt_details_map_[connection_details].insert(
      connection_attempt_details);

  // Process the role-specific details.
  switch (connection_attempt_details.connection_medium()) {
    case ConnectionMedium::kBluetoothLowEnergy:
      HandleBleRequest(connection_attempt_details,
                       std::move(client_connection_parameters),
                       connection_priority);
      break;
    case ConnectionMedium::kNearbyConnections:
      HandleNearbyRequest(connection_attempt_details,
                          std::move(client_connection_parameters),
                          connection_priority);
      break;
  }
}

void PendingConnectionManagerImpl::OnConnectionAttemptSucceeded(
    const ConnectionDetails& connection_details,
    std::unique_ptr<AuthenticatedChannel> authenticated_channel) {
  if (!base::Contains(details_to_attempt_details_map_, connection_details)) {
    PA_LOG(ERROR) << "PendingConnectionManagerImpl::"
                  << "OnConnectionAttemptSucceeded(): Attempt succeeded, but "
                  << "there was no corresponding map entry. "
                  << "Details: " << connection_details;
    NOTREACHED_IN_MIGRATION();
  }

  std::vector<std::unique_ptr<ClientConnectionParameters>> all_clients;

  // Make a copy of the ConnectionAttemptDetails to process to prevent modifying
  // the set during iteration.
  base::flat_set<ConnectionAttemptDetails> to_process =
      details_to_attempt_details_map_[connection_details];

  // For each associated ConnectionAttemptDetails, extract clients from the
  // connection attempt, add them to |all_clients|, and remove the associated
  // map entries.
  for (const auto& connection_attempt_details : to_process) {
    std::vector<std::unique_ptr<ClientConnectionParameters>> clients_in_loop =
        ExtractClientConnectionParameters(connection_attempt_details);

    // Move elements in |clients_in_list| to |all_clients|.
    all_clients.insert(all_clients.end(),
                       std::make_move_iterator(clients_in_loop.begin()),
                       std::make_move_iterator(clients_in_loop.end()));

    RemoveMapEntriesForFinishedConnectionAttempt(connection_attempt_details);
  }

  NotifyOnConnection(std::move(authenticated_channel), std::move(all_clients),
                     connection_details);
}

void PendingConnectionManagerImpl::OnConnectionAttemptFinishedWithoutConnection(
    const ConnectionAttemptDetails& connection_attempt_details) {
  RemoveMapEntriesForFinishedConnectionAttempt(connection_attempt_details);
}

void PendingConnectionManagerImpl::HandleBleRequest(
    const ConnectionAttemptDetails& connection_attempt_details,
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    ConnectionPriority connection_priority) {
  switch (connection_attempt_details.connection_role()) {
    case ConnectionRole::kInitiatorRole:
      HandleBleInitiatorRequest(connection_attempt_details,
                                std::move(client_connection_parameters),
                                connection_priority);
      break;
    case ConnectionRole::kListenerRole:
      HandleBleListenerRequest(connection_attempt_details,
                               std::move(client_connection_parameters),
                               connection_priority);
      break;
  }
}

void PendingConnectionManagerImpl::HandleBleInitiatorRequest(
    const ConnectionAttemptDetails& connection_attempt_details,
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    ConnectionPriority connection_priority) {
  // If no ConnectionAttempt exists to this device in the initiator role, create
  // one.
  if (!base::Contains(id_pair_to_ble_initiator_connection_attempts_,
                      connection_attempt_details.device_id_pair())) {
    id_pair_to_ble_initiator_connection_attempts_[connection_attempt_details
                                                      .device_id_pair()] =
        BleInitiatorConnectionAttempt::Factory::Create(
            ble_connection_manager_, this /* delegate */,
            connection_attempt_details);
  }

  auto& connection_attempt =
      id_pair_to_ble_initiator_connection_attempts_[connection_attempt_details
                                                        .device_id_pair()];

  bool success = connection_attempt->AddPendingConnectionRequest(
      PendingBleInitiatorConnectionRequest::Factory::Create(
          std::move(client_connection_parameters), connection_priority,
          connection_attempt.get() /* delegate */, bluetooth_adapter_));

  if (!success) {
    PA_LOG(ERROR) << "PendingConnectionManagerImpl::"
                  << "HandleBleInitiatorRequest(): Not able to handle request. "
                  << "Details: " << connection_attempt_details;
    NOTREACHED_IN_MIGRATION();
  }
}

void PendingConnectionManagerImpl::HandleBleListenerRequest(
    const ConnectionAttemptDetails& connection_attempt_details,
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    ConnectionPriority connection_priority) {
  // If no ConnectionAttempt exists to this device in the listener role, create
  // one.
  if (!base::Contains(id_pair_to_ble_listener_connection_attempts_,
                      connection_attempt_details.device_id_pair())) {
    id_pair_to_ble_listener_connection_attempts_[connection_attempt_details
                                                     .device_id_pair()] =
        BleListenerConnectionAttempt::Factory::Create(
            ble_connection_manager_, this /* delegate */,
            connection_attempt_details);
  }

  auto& connection_attempt =
      id_pair_to_ble_listener_connection_attempts_[connection_attempt_details
                                                       .device_id_pair()];

  bool success = connection_attempt->AddPendingConnectionRequest(
      PendingBleListenerConnectionRequest::Factory::Create(
          std::move(client_connection_parameters), connection_priority,
          connection_attempt.get() /* delegate */, bluetooth_adapter_));

  if (!success) {
    PA_LOG(ERROR) << "PendingConnectionManagerImpl::"
                  << "HandleBleListenerRequest(): Not able to handle request. "
                  << "Details: " << connection_attempt_details;
    NOTREACHED_IN_MIGRATION();
  }
}

void PendingConnectionManagerImpl::HandleNearbyRequest(
    const ConnectionAttemptDetails& connection_attempt_details,
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    ConnectionPriority connection_priority) {
  switch (connection_attempt_details.connection_role()) {
    case ConnectionRole::kInitiatorRole:
      HandleNearbyInitiatorRequest(connection_attempt_details,
                                   std::move(client_connection_parameters),
                                   connection_priority);
      break;
    case ConnectionRole::kListenerRole:
      NOTREACHED_IN_MIGRATION()
          << "PendingConnectionManagerImpl::HandleConnectionRequest(): "
          << "Nearby Connections is not supported in the listener role.";
      break;
  }
}

void PendingConnectionManagerImpl::HandleNearbyInitiatorRequest(
    const ConnectionAttemptDetails& connection_attempt_details,
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    ConnectionPriority connection_priority) {
  // If no ConnectionAttempt exists to this device in the initiator role, create
  // one.
  if (!base::Contains(id_pair_to_nearby_initiator_connection_attempts_,
                      connection_attempt_details.device_id_pair())) {
    id_pair_to_nearby_initiator_connection_attempts_[connection_attempt_details
                                                         .device_id_pair()] =
        NearbyInitiatorConnectionAttempt::Factory::Create(
            nearby_connection_manager_, this /* delegate */,
            connection_attempt_details);
  }

  auto& connection_attempt = id_pair_to_nearby_initiator_connection_attempts_
      [connection_attempt_details.device_id_pair()];

  bool success = connection_attempt->AddPendingConnectionRequest(
      PendingNearbyInitiatorConnectionRequest::Factory::Create(
          std::move(client_connection_parameters), connection_priority,
          connection_attempt.get() /* delegate */, bluetooth_adapter_));

  if (!success) {
    PA_LOG(ERROR)
        << "PendingConnectionManagerImpl::"
        << "HandleNearbyInitiatorRequest(): Not able to handle request. "
        << "Details: " << connection_attempt_details;
    NOTREACHED_IN_MIGRATION();
  }
}

std::vector<std::unique_ptr<ClientConnectionParameters>>
PendingConnectionManagerImpl::ExtractClientConnectionParameters(
    const ConnectionAttemptDetails& connection_attempt_details) {
  switch (connection_attempt_details.connection_medium()) {
    case ConnectionMedium::kBluetoothLowEnergy:
      switch (connection_attempt_details.connection_role()) {
        // BLE initiator:
        case ConnectionRole::kInitiatorRole:
          return ConnectionAttempt<BleInitiatorFailureType>::
              ExtractClientConnectionParameters(
                  std::move(id_pair_to_ble_initiator_connection_attempts_
                                [connection_attempt_details.device_id_pair()]));

        // BLE listener:
        case ConnectionRole::kListenerRole:
          return ConnectionAttempt<BleListenerFailureType>::
              ExtractClientConnectionParameters(
                  std::move(id_pair_to_ble_listener_connection_attempts_
                                [connection_attempt_details.device_id_pair()]));
      }

    case ConnectionMedium::kNearbyConnections:
      switch (connection_attempt_details.connection_role()) {
        // Nearby initiator:
        case ConnectionRole::kInitiatorRole:
          return ConnectionAttempt<NearbyInitiatorFailureType>::
              ExtractClientConnectionParameters(
                  std::move(id_pair_to_nearby_initiator_connection_attempts_
                                [connection_attempt_details.device_id_pair()]));

        // Nearby listener:
        case ConnectionRole::kListenerRole:
          NOTREACHED_IN_MIGRATION()
              << "Nearby listener connections are not implemented.";
          return std::vector<std::unique_ptr<ClientConnectionParameters>>();
      }
  }
}

void PendingConnectionManagerImpl::RemoveMapEntriesForFinishedConnectionAttempt(
    const ConnectionAttemptDetails& connection_attempt_details) {
  // Make a copy of |connection_attempt_details|, since it belongs to the
  // ConnectionAttempt which is about to be deleted below.
  ConnectionAttemptDetails connection_attempt_details_copy =
      connection_attempt_details;
  ConnectionDetails connection_details =
      connection_attempt_details_copy.GetAssociatedConnectionDetails();

  RemoveIdPairToConnectionAttemptMapEntriesForFinishedConnectionAttempt(
      connection_attempt_details_copy);

  size_t num_deleted =
      details_to_attempt_details_map_[connection_details].erase(
          connection_attempt_details_copy);
  if (num_deleted != 1u) {
    PA_LOG(ERROR) << "PendingConnectionManagerImpl::"
                  << "RemoveMapEntriesForFinishedConnectionAttempt(): "
                  << "Tried to remove ConnectionAttemptDetails, but they were"
                  << "not present in the map. Details: "
                  << connection_attempt_details_copy;
    NOTREACHED_IN_MIGRATION();
  }

  // If |connection_attempt_details_copy| was the last entry, remove the entire
  // set.
  if (details_to_attempt_details_map_[connection_details].empty())
    details_to_attempt_details_map_.erase(connection_details);
}

void PendingConnectionManagerImpl::
    RemoveIdPairToConnectionAttemptMapEntriesForFinishedConnectionAttempt(
        const ConnectionAttemptDetails& connection_attempt_details) {
  switch (connection_attempt_details.connection_medium()) {
    case ConnectionMedium::kBluetoothLowEnergy:
      switch (connection_attempt_details.connection_role()) {
        // BLE initiator.
        case ConnectionRole::kInitiatorRole: {
          size_t num_deleted =
              id_pair_to_ble_initiator_connection_attempts_.erase(
                  connection_attempt_details.device_id_pair());
          if (num_deleted != 1u) {
            PA_LOG(ERROR) << "Tried to remove failed BLE initiator "
                          << "ConnectionAttempt, but it was not present in the "
                          << "map. Details: " << connection_attempt_details;
            NOTREACHED_IN_MIGRATION();
          }
          break;
        }

        // BLE listener.
        case ConnectionRole::kListenerRole: {
          size_t num_deleted =
              id_pair_to_ble_listener_connection_attempts_.erase(
                  connection_attempt_details.device_id_pair());
          if (num_deleted != 1u) {
            PA_LOG(ERROR) << "Tried to remove failed BLE listener "
                          << "ConnectionAttempt, but it was not present in the "
                          << "map. Details: " << connection_attempt_details;
            NOTREACHED_IN_MIGRATION();
          }
          break;
        }
      }
      break;

    case ConnectionMedium::kNearbyConnections:
      switch (connection_attempt_details.connection_role()) {
        // Nearby initiator.
        case ConnectionRole::kInitiatorRole: {
          size_t num_deleted =
              id_pair_to_nearby_initiator_connection_attempts_.erase(
                  connection_attempt_details.device_id_pair());
          if (num_deleted != 1u) {
            PA_LOG(ERROR) << "Tried to remove failed Nearby initiator "
                          << "ConnectionAttempt, but it was not present in the "
                          << "map. Details: " << connection_attempt_details;
            NOTREACHED_IN_MIGRATION();
          }
          break;
        }

        case ConnectionRole::kListenerRole:
          NOTREACHED_IN_MIGRATION();
          break;
      }
      break;
  }
}

}  // namespace ash::secure_channel
