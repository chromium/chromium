// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_MANAGER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_MANAGER_IMPL_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/secure_channel/ble_initiator_failure_type.h"
#include "chromeos/ash/services/secure_channel/ble_listener_failure_type.h"
#include "chromeos/ash/services/secure_channel/client_connection_parameters.h"
#include "chromeos/ash/services/secure_channel/connection_attempt.h"
#include "chromeos/ash/services/secure_channel/connection_attempt_delegate.h"
#include "chromeos/ash/services/secure_channel/connection_role.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"
#include "chromeos/ash/services/secure_channel/nearby_initiator_failure_type.h"
#include "chromeos/ash/services/secure_channel/pending_connection_manager.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_medium.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash::secure_channel {

class BleConnectionManager;
class NearbyConnectionManager;

// Concrete PendingConnectionManager implementation. This class creates one
// ConnectionAttempt per ConnectionAttemptDetails requested; if more than one
// request shares the same ConnectionAttemptDetails, a single ConnectionAttempt
// attempts a connection for all associated requests.
//
// If a ConnectionAttempt successfully creates a channel, this class extracts
// client data from all requests to the same remote device and alerts its
// delegate, deleting all associated ConnectionAttempts when it is finished.
class PendingConnectionManagerImpl : public PendingConnectionManager,
                                     public ConnectionAttemptDelegate {
 public:
  class Factory {
   public:
    static std::unique_ptr<PendingConnectionManager> Create(
        Delegate* delegate,
        BleConnectionManager* ble_connection_manager,
        NearbyConnectionManager* nearby_connection_manager,
        scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<PendingConnectionManager> CreateInstance(
        Delegate* delegate,
        BleConnectionManager* ble_connection_manager,
        NearbyConnectionManager* nearby_connection_manager,
        scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) = 0;

   private:
    static Factory* test_factory_;
  };

  PendingConnectionManagerImpl(const PendingConnectionManagerImpl&) = delete;
  PendingConnectionManagerImpl& operator=(const PendingConnectionManagerImpl&) =
      delete;

  ~PendingConnectionManagerImpl() override;

 private:
  PendingConnectionManagerImpl(
      Delegate* delegate,
      BleConnectionManager* ble_connection_manager,
      NearbyConnectionManager* nearby_connection_manager,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);

  // PendingConnectionManager:
  void HandleConnectionRequest(
      const ConnectionAttemptDetails& connection_attempt_details,
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority) override;

  // ConnectionAttemptDelegate:
  void OnConnectionAttemptSucceeded(
      const ConnectionDetails& connection_details,
      std::unique_ptr<AuthenticatedChannel> authenticated_channel) override;
  void OnConnectionAttemptFinishedWithoutConnection(
      const ConnectionAttemptDetails& connection_attempt_details) override;

  void HandleBleRequest(
      const ConnectionAttemptDetails& connection_attempt_details,
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority);
  void HandleBleInitiatorRequest(
      const ConnectionAttemptDetails& connection_attempt_details,
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority);
  void HandleBleListenerRequest(
      const ConnectionAttemptDetails& connection_attempt_details,
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority);

  void HandleNearbyRequest(
      const ConnectionAttemptDetails& connection_attempt_details,
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority);
  void HandleNearbyInitiatorRequest(
      const ConnectionAttemptDetails& connection_attempt_details,
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority);

  // Retrieves ClientConnectionParameters for a given connection attempt.
  // Because a single connection attempt may have multiple client requests
  // (e.g., when multiple clients requets a connection at the same time), this
  // function returns a vector.
  //
  // Note that this function std::move()s results from the |id_pair_to_*_| maps
  // below, so these maps will end up having "empty" values after the function
  // is called. This function is expected to be used in conjunction with
  // RemoveMapEntriesForFinishedConnectionAttempt(), which cleans up those empty
  // values.
  std::vector<std::unique_ptr<ClientConnectionParameters>>
  ExtractClientConnectionParameters(
      const ConnectionAttemptDetails& connection_attempt_details);

  void RemoveMapEntriesForFinishedConnectionAttempt(
      const ConnectionAttemptDetails& connection_attempt_details);
  void RemoveIdPairToConnectionAttemptMapEntriesForFinishedConnectionAttempt(
      const ConnectionAttemptDetails& connection_attempt_details);

  base::flat_map<DeviceIdPair,
                 std::unique_ptr<ConnectionAttempt<BleInitiatorFailureType>>>
      id_pair_to_ble_initiator_connection_attempts_;

  base::flat_map<DeviceIdPair,
                 std::unique_ptr<ConnectionAttempt<BleListenerFailureType>>>
      id_pair_to_ble_listener_connection_attempts_;

  base::flat_map<DeviceIdPair,
                 std::unique_ptr<ConnectionAttempt<NearbyInitiatorFailureType>>>
      id_pair_to_nearby_initiator_connection_attempts_;

  base::flat_map<ConnectionDetails, base::flat_set<ConnectionAttemptDetails>>
      details_to_attempt_details_map_;

  raw_ptr<BleConnectionManager> ble_connection_manager_;
  raw_ptr<NearbyConnectionManager> nearby_connection_manager_;
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_MANAGER_IMPL_H_
