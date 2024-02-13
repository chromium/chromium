// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_MANAGER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_MANAGER_IMPL_H_

#include <optional>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/secure_channel/ble_scanner.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"
#include "chromeos/ash/services/secure_channel/nearby_connection_manager.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/secure_channel.h"

namespace ash::secure_channel {

class SecureChannelDisconnector;

// NearbyConnectionManager implementation which uses BleScanner to determine
// whether the desired device is in proximity. If BleScanner discovers the
// device is nearby, it creates a new NearbyConnection to that device, then
// completes the authentication flow before returning it to the caller.
class NearbyConnectionManagerImpl : public NearbyConnectionManager,
                                    public BleScanner::Observer,
                                    public SecureChannel::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<NearbyConnectionManager> Create(
        BleScanner* ble_scanner,
        SecureChannelDisconnector* secure_channel_disconnector);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<NearbyConnectionManager> CreateInstance(
        BleScanner* ble_scanner,
        SecureChannelDisconnector* secure_channel_disconnector) = 0;

   private:
    static Factory* test_factory_;
  };

  NearbyConnectionManagerImpl(const NearbyConnectionManagerImpl&) = delete;
  NearbyConnectionManagerImpl& operator=(const NearbyConnectionManagerImpl&) =
      delete;
  ~NearbyConnectionManagerImpl() override;

 private:
  NearbyConnectionManagerImpl(
      BleScanner* ble_scanner,
      SecureChannelDisconnector* secure_channel_disconnector);

  // NearbyConnectionManager:
  void PerformAttemptNearbyInitiatorConnection(
      const DeviceIdPair& device_id_pair) override;
  void PerformCancelNearbyInitiatorConnectionAttempt(
      const DeviceIdPair& device_id_pair) override;

  // BleScanner::Observer:
  void OnReceivedAdvertisement(multidevice::RemoteDeviceRef remote_device,
                               device::BluetoothDevice* bluetooth_device,
                               ConnectionMedium connection_medium,
                               ConnectionRole connection_role,
                               const std::vector<uint8_t>& eid) override;
  void OnDiscoveryFailed(
      const DeviceIdPair& device_id_pair,
      mojom::DiscoveryResult discovery_result,
      std::optional<mojom::DiscoveryErrorCode> potential_error_code) override;

  // SecureChannel::Observer:
  void OnSecureChannelStatusChanged(
      SecureChannel* secure_channel,
      const SecureChannel::Status& old_status,
      const SecureChannel::Status& new_status) override;
  void OnNearbyConnectionStateChanged(
      SecureChannel* secure_channel,
      mojom::NearbyConnectionStep step,
      mojom::NearbyConnectionStepResult result) override;
  void OnSecureChannelAuthenticationStateChanged(
      SecureChannel* secure_channel,
      mojom::SecureChannelState secure_channel_state) override;

  // Returns whether a channel exists connecting to |remote_device_id|,
  // regardless of the local device ID used to create the connection.
  bool DoesAuthenticatingChannelExist(const std::string& remote_device_id);

  // Adds |secure_channel| to |remote_device_id_to_secure_channel_map_| and
  // pauses any ongoing attempts to |remote_device_id|, since a connection has
  // already been established to that device.
  void SetAuthenticatingChannel(const std::string& remote_device_id,
                                std::unique_ptr<SecureChannel> secure_channel);

  // Pauses pending connection attempts (i.e., BLE scanning) for
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
  // that two attempts are made with the same remote device ID but different
  // local device IDs. In the case of multiple possible recipients, we
  // arbitrarily choose the one which was registered first.
  DeviceIdPair ChooseChannelRecipient(const std::string& remote_device_id);

  raw_ptr<BleScanner> ble_scanner_;
  raw_ptr<SecureChannelDisconnector> secure_channel_disconnector_;

  base::flat_map<std::string, std::unique_ptr<SecureChannel>>
      remote_device_id_to_secure_channel_map_;
  std::optional<std::string> notifying_remote_device_id_;
  base::flat_set<DeviceIdPair> discovered_device_id_pair_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_MANAGER_IMPL_H_
