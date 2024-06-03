// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_CONNECTION_MANAGER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_CONNECTION_MANAGER_IMPL_H_

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chromeos/ash/services/secure_channel/ble_advertiser.h"
#include "chromeos/ash/services/secure_channel/ble_connection_manager.h"
#include "chromeos/ash/services/secure_channel/ble_scanner.h"
#include "chromeos/ash/services/secure_channel/connection_role.h"
#include "chromeos/ash/services/secure_channel/secure_channel.h"

namespace ash::timer_factory {
class TimerFactory;
}  // namespace ash::timer_factory

namespace device {
class BluetoothAdapter;
}

namespace ash::secure_channel {

class BleSynchronizerBase;
class BluetoothHelper;
class SecureChannelDisconnector;

// Concrete BleConnectionManager implementation. This class initializes
// BleAdvertiser and BleScanner objects and utilizes them to bootstrap
// connections. Once a connection is found, BleConnectionManagerImpl creates a
// SecureChannel and waits for it to authenticate successfully. Once
// this process is complete, an AuthenticatedChannel is returned to the client.
class BleConnectionManagerImpl : public BleConnectionManager,
                                 public BleAdvertiser::Delegate,
                                 public BleScanner::Observer,
                                 public SecureChannel::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<BleConnectionManager> Create(
        scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
        BluetoothHelper* bluetooth_helper,
        BleSynchronizerBase* ble_synchronizer,
        BleScanner* ble_scanner,
        SecureChannelDisconnector* secure_channel_disconnector,
        ash::timer_factory::TimerFactory* timer_factory,
        base::Clock* clock = base::DefaultClock::GetInstance());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<BleConnectionManager> CreateInstance(
        scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
        BluetoothHelper* bluetooth_helper,
        BleSynchronizerBase* ble_synchronizer,
        BleScanner* ble_scanner,
        SecureChannelDisconnector* secure_channel_disconnector,
        ash::timer_factory::TimerFactory* timer_factory,
        base::Clock* clock = base::DefaultClock::GetInstance()) = 0;

   private:
    static Factory* test_factory_;
  };

  BleConnectionManagerImpl(const BleConnectionManagerImpl&) = delete;
  BleConnectionManagerImpl& operator=(const BleConnectionManagerImpl&) = delete;

  ~BleConnectionManagerImpl() override;

 private:
  class ConnectionAttemptTimestamps {
   public:
    ConnectionAttemptTimestamps(ConnectionRole connection_role,
                                base::Clock* clock);
    ~ConnectionAttemptTimestamps();

    void RecordAdvertisementReceived();
    void RecordGattConnectionEstablished();
    void RecordChannelAuthenticated();

    // Resets the connection attempt metrics by setting the "start scan"
    // timestamp to the current time and by and nulling out the other
    // timestamps.
    void Reset();

   private:
    void RecordEffectiveSuccessRateMetrics(bool will_continue_to_retry);

    const ConnectionRole connection_role_;
    raw_ptr<base::Clock> clock_;

    // Set to the current time when this object is created and updated whenever
    // Reset() is called.
    base::Time start_scan_timestamp_;

    // Start as null timestamps and are set whenever the relevant event occurs;
    // if Reset() is called, these timestamps are nulled out again.
    base::Time advertisement_received_timestamp_;
    base::Time gatt_connection_timestamp_;
    base::Time authentication_timestamp_;
  };

  BleConnectionManagerImpl(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      BluetoothHelper* bluetooth_helper,
      BleSynchronizerBase* ble_synchronizer,
      BleScanner* ble_scanner,
      SecureChannelDisconnector* secure_channel_disconnector,
      ash::timer_factory::TimerFactory* timer_factory,
      base::Clock* clock);

  // BleConnectionManager:
  void PerformAttemptBleInitiatorConnection(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) override;
  void PerformUpdateBleInitiatorConnectionPriority(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) override;
  void PerformCancelBleInitiatorConnectionAttempt(
      const DeviceIdPair& device_id_pair) override;
  void PerformAttemptBleListenerConnection(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) override;
  void PerformUpdateBleListenerConnectionPriority(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) override;
  void PerformCancelBleListenerConnectionAttempt(
      const DeviceIdPair& device_id_pair) override;

  // BleAdvertiser::Delegate:
  void OnAdvertisingSlotEnded(
      const DeviceIdPair& device_id_pair,
      bool replaced_by_higher_priority_advertisement) override;
  void OnFailureToGenerateAdvertisement(
      const DeviceIdPair& device_id_pair) override;

  // BleScanner::Observer:
  void OnReceivedAdvertisement(multidevice::RemoteDeviceRef remote_device,
                               device::BluetoothDevice* bluetooth_device,
                               ConnectionMedium connection_medium,
                               ConnectionRole connection_role,
                               const std::vector<uint8_t>& eid) override;

  // SecureChannel::Observer:
  void OnSecureChannelStatusChanged(
      SecureChannel* secure_channel,
      const SecureChannel::Status& old_status,
      const SecureChannel::Status& new_status) override;

  // Returns whether a channel exists connecting to |remote_device_id|,
  // regardless of the local device ID or the role used to create the
  // connection.
  bool DoesAuthenticatingChannelExist(const std::string& remote_device_id);

  // Adds |secure_channel| to |remote_device_id_to_secure_channel_map_| and
  // pauses any ongoing attempts to |remote_device_id|, since a connection has
  // already been established to that device.
  void SetAuthenticatingChannel(const std::string& remote_device_id,
                                std::unique_ptr<SecureChannel> secure_channel,
                                ConnectionRole connection_role);

  // Pauses pending connection attempts (scanning and/or advertising) to
  // |remote_device_id|.
  void PauseConnectionAttemptsToDevice(const std::string& remote_device_id);

  // Restarts connections which were paused as part of
  // PauseConnectionAttemptsToDevice();
  void RestartPausedAttemptsToDevice(const std::string& remote_device_id);

  // Checks to see if there is a leftover channel authenticating with
  // |remote_device_id| even though there are no pending requests for a
  // connection to that device. This situation arises when an active request is
  // canceled after a connection has been established but before that connection
  // has been fully authenticated. This function disconnects the channel in the
  // case that it finds one.
  void ProcessPotentialLingeringChannel(const std::string& remote_device_id);

  std::string GetRemoteDeviceIdForSecureChannel(SecureChannel* secure_channel);
  void HandleSecureChannelDisconnection(const std::string& remote_device_id,
                                        bool was_authenticating);
  void HandleChannelAuthenticated(const std::string& remote_device_id);

  // Chooses the connection attempt which will receive the success callback.
  // It is possible that there is more than one possible recipient in the case
  // that two attempts are made with the same remote device ID and connection
  // role but different local device IDs. In the case of multiple possible
  // recipients, we arbitrarily choose the one which was registered first.
  ConnectionAttemptDetails ChooseChannelRecipient(
      const std::string& remote_device_id,
      ConnectionRole connection_role);

  // Starts tracking a connection attempt's duration. If a connection to
  // |remote_device_id| is already in progress, this function is a no-op.
  void StartConnectionAttemptTimerMetricsIfNecessary(
      const std::string& remote_device_id,
      ConnectionRole connection_role);

  // Removes tracking for a connection attempt's duration if there are no
  // remaining requests for the connection.
  void RemoveConnectionAttemptTimerMetricsIfNecessary(
      const std::string& remote_device_id);

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  raw_ptr<base::Clock> clock_;
  raw_ptr<BleScanner> ble_scanner_;
  raw_ptr<SecureChannelDisconnector> secure_channel_disconnector_;

  std::unique_ptr<BleAdvertiser> ble_advertiser_;

  using SecureChannelWithRole =
      std::pair<std::unique_ptr<SecureChannel>, ConnectionRole>;
  base::flat_map<std::string, SecureChannelWithRole>
      remote_device_id_to_secure_channel_map_;
  base::flat_map<std::string, std::unique_ptr<ConnectionAttemptTimestamps>>
      remote_device_id_to_timestamps_map_;
  std::optional<std::string> notifying_remote_device_id_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_CONNECTION_MANAGER_IMPL_H_
