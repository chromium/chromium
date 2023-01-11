// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_CONNECTION_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_CONNECTION_MANAGER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "chromeos/ash/services/secure_channel/ble_initiator_failure_type.h"
#include "chromeos/ash/services/secure_channel/ble_listener_failure_type.h"
#include "chromeos/ash/services/secure_channel/connection_attempt_details.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"

namespace ash::secure_channel {

class AuthenticatedChannel;
enum class ConnectionRole;

// Creates connections to remote devices over Bluetooth, using either the
// listener role (BLE scans only) or the initiator role (a combination of BLE
// advertising and scanning).
//
// When a connection is attempted, it remains active until either an
// AuthenticatedChannel is returned successfully or until the request is
// explicitly removed via one of the Cancel*() functions.
//
// When a failure occurs, the client is notified, but the connection attempt
// remains active. This ensures that when attempts are retried after a failure,
// this class does not need to internally stop and then restart
// scanning/advertising.
class BleConnectionManager {
 public:
  BleConnectionManager(const BleConnectionManager&) = delete;
  BleConnectionManager& operator=(const BleConnectionManager&) = delete;

  virtual ~BleConnectionManager();

  using ConnectionSuccessCallback =
      base::OnceCallback<void(std::unique_ptr<AuthenticatedChannel>)>;
  using BleInitiatorFailureCallback =
      base::RepeatingCallback<void(BleInitiatorFailureType)>;
  using BleListenerFailureCallback =
      base::RepeatingCallback<void(BleListenerFailureType)>;

  void AttemptBleInitiatorConnection(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority,
      ConnectionSuccessCallback success_callback,
      const BleInitiatorFailureCallback& failure_callback);

  void UpdateBleInitiatorConnectionPriority(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority);

  void CancelBleInitiatorConnectionAttempt(const DeviceIdPair& device_id_pair);

  void AttemptBleListenerConnection(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority,
      ConnectionSuccessCallback success_callback,
      const BleListenerFailureCallback& failure_callback);

  void UpdateBleListenerConnectionPriority(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority);

  void CancelBleListenerConnectionAttempt(const DeviceIdPair& device_id_pair);

 protected:
  BleConnectionManager();

  virtual void PerformAttemptBleInitiatorConnection(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) = 0;
  virtual void PerformUpdateBleInitiatorConnectionPriority(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) = 0;
  virtual void PerformCancelBleInitiatorConnectionAttempt(
      const DeviceIdPair& device_id_pair) = 0;
  virtual void PerformAttemptBleListenerConnection(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) = 0;
  virtual void PerformUpdateBleListenerConnectionPriority(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) = 0;
  virtual void PerformCancelBleListenerConnectionAttempt(
      const DeviceIdPair& device_id_pair) = 0;

  ConnectionPriority GetPriorityForAttempt(const DeviceIdPair& device_id_pair,
                                           ConnectionRole connection_role);
  const base::flat_set<ConnectionAttemptDetails>& GetDetailsForRemoteDevice(
      const std::string& remote_device_id);
  bool DoesAttemptExist(const DeviceIdPair& device_id_pair,
                        ConnectionRole connection_role);

  void NotifyBleInitiatorFailure(const DeviceIdPair& device_id_pair,
                                 BleInitiatorFailureType failure_type);
  void NotifyBleListenerFailure(const DeviceIdPair& device_id_pair,
                                BleListenerFailureType failure_type);
  void NotifyConnectionSuccess(
      const DeviceIdPair& device_id_pair,
      ConnectionRole connection_role,
      std::unique_ptr<AuthenticatedChannel> authenticated_channel);

 private:
  struct InitiatorConnectionAttemptMetadata {
    InitiatorConnectionAttemptMetadata(
        ConnectionPriority connection_priority,
        ConnectionSuccessCallback success_callback,
        const BleInitiatorFailureCallback& failure_callback);
    ~InitiatorConnectionAttemptMetadata();

    ConnectionPriority connection_priority;
    ConnectionSuccessCallback success_callback;
    BleInitiatorFailureCallback failure_callback;
  };

  struct ListenerConnectionAttemptMetadata {
    ListenerConnectionAttemptMetadata(
        ConnectionPriority connection_priority,
        ConnectionSuccessCallback success_callback,
        const BleListenerFailureCallback& failure_callback);
    ~ListenerConnectionAttemptMetadata();

    ConnectionPriority connection_priority;
    ConnectionSuccessCallback success_callback;
    BleListenerFailureCallback failure_callback;
  };

  InitiatorConnectionAttemptMetadata& GetInitiatorEntry(
      const DeviceIdPair& device_id_pair);
  ListenerConnectionAttemptMetadata& GetListenerEntry(
      const DeviceIdPair& device_id_pair);
  void RemoveRequestMetadata(const DeviceIdPair& device_id_pair,
                             ConnectionRole connection_role);

  base::flat_map<std::string, base::flat_set<ConnectionAttemptDetails>>
      remote_device_id_to_details_map_;
  base::flat_map<DeviceIdPair,
                 std::unique_ptr<InitiatorConnectionAttemptMetadata>>
      id_pair_to_initiator_metadata_map_;
  base::flat_map<DeviceIdPair,
                 std::unique_ptr<ListenerConnectionAttemptMetadata>>
      id_pair_to_listener_metadata_map_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_CONNECTION_MANAGER_H_
