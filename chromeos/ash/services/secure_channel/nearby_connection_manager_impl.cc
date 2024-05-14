// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/nearby_connection_manager_impl.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/secure_channel/authenticated_channel_impl.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"
#include "chromeos/ash/services/secure_channel/nearby_connection.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/ash/services/secure_channel/secure_channel_disconnector.h"

namespace ash::secure_channel {

// static
NearbyConnectionManagerImpl::Factory*
    NearbyConnectionManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyConnectionManager>
NearbyConnectionManagerImpl::Factory::Create(
    BleScanner* ble_scanner,
    SecureChannelDisconnector* secure_channel_disconnector) {
  if (test_factory_) {
    return test_factory_->CreateInstance(ble_scanner,
                                         secure_channel_disconnector);
  }

  return base::WrapUnique(new NearbyConnectionManagerImpl(
      ble_scanner, secure_channel_disconnector));
}

// static
void NearbyConnectionManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyConnectionManagerImpl::Factory::~Factory() = default;

NearbyConnectionManagerImpl::NearbyConnectionManagerImpl(
    BleScanner* ble_scanner,
    SecureChannelDisconnector* secure_channel_disconnector)
    : ble_scanner_(ble_scanner),
      secure_channel_disconnector_(secure_channel_disconnector) {
  ble_scanner_->AddObserver(this);
}

NearbyConnectionManagerImpl::~NearbyConnectionManagerImpl() {
  ble_scanner_->RemoveObserver(this);
}

void NearbyConnectionManagerImpl::PerformAttemptNearbyInitiatorConnection(
    const DeviceIdPair& device_id_pair) {
  if (DoesAuthenticatingChannelExist(device_id_pair.remote_device_id()))
    return;

  ble_scanner_->AddScanRequest(ConnectionAttemptDetails(
      device_id_pair, ConnectionMedium::kNearbyConnections,
      ConnectionRole::kInitiatorRole));
}

void NearbyConnectionManagerImpl::PerformCancelNearbyInitiatorConnectionAttempt(
    const DeviceIdPair& device_id_pair) {
  if (DoesAuthenticatingChannelExist(device_id_pair.remote_device_id())) {
    // Check to see if we are removing the final request for an active channel;
    // if so, that channel needs to be disconnected.
    ProcessPotentialLingeringChannel(device_id_pair.remote_device_id());
    return;
  }

  // If a client canceled its request as a result of being notified of an
  // authenticated channel, that request was not actually active.
  if (notifying_remote_device_id_ == device_id_pair.remote_device_id())
    return;
  ble_scanner_->RemoveScanRequest(ConnectionAttemptDetails(
      device_id_pair, ConnectionMedium::kNearbyConnections,
      ConnectionRole::kInitiatorRole));
}

void NearbyConnectionManagerImpl::OnReceivedAdvertisement(
    multidevice::RemoteDeviceRef remote_device,
    device::BluetoothDevice* bluetooth_device,
    ConnectionMedium connection_medium,
    ConnectionRole connection_role,
    const std::vector<uint8_t>& eid) {
  // Only process advertisements received as part of the Nearby Connections
  // flow.
  if (connection_medium != ConnectionMedium::kNearbyConnections)
    return;

  // Create a connection to the device.
  std::unique_ptr<Connection> connection = NearbyConnection::Factory::Create(
      remote_device, eid, GetNearbyConnector());

  NotifyBleDiscoveryStateChanged(
      ChooseChannelRecipient(remote_device.GetDeviceId()),
      mojom::DiscoveryResult::kSuccess, std::nullopt);
  SetAuthenticatingChannel(
      remote_device.GetDeviceId(),
      SecureChannel::Factory::Create(std::move(connection)));
}

void NearbyConnectionManagerImpl::OnDiscoveryFailed(
    const DeviceIdPair& device_id_pair,
    mojom::DiscoveryResult discovery_result,
    std::optional<mojom::DiscoveryErrorCode> potential_error_code) {
  NotifyBleDiscoveryStateChanged(device_id_pair, discovery_result,
                                 potential_error_code);
}

void NearbyConnectionManagerImpl::OnSecureChannelStatusChanged(
    SecureChannel* secure_channel,
    const SecureChannel::Status& old_status,
    const SecureChannel::Status& new_status) {
  std::string remote_device_id =
      GetRemoteDeviceIdForSecureChannel(secure_channel);

  if (new_status == SecureChannel::Status::DISCONNECTED) {
    bool was_authenticating =
        old_status == SecureChannel::Status::AUTHENTICATING;
    HandleSecureChannelDisconnection(remote_device_id, was_authenticating);
    return;
  }

  if (new_status == SecureChannel::Status::AUTHENTICATED)
    HandleChannelAuthenticated(remote_device_id);
}

void NearbyConnectionManagerImpl::OnNearbyConnectionStateChanged(
    SecureChannel* secure_channel,
    mojom::NearbyConnectionStep step,
    mojom::NearbyConnectionStepResult result) {
  std::string remote_device_id =
      GetRemoteDeviceIdForSecureChannel(secure_channel);
  NotifyNearbyConnectionStateChanged(ChooseChannelRecipient(remote_device_id),
                                     step, result);
}

void NearbyConnectionManagerImpl::OnSecureChannelAuthenticationStateChanged(
    SecureChannel* secure_channel,
    mojom::SecureChannelState secure_channel_state) {
  std::string remote_device_id =
      GetRemoteDeviceIdForSecureChannel(secure_channel);
  NotifySecureChannelAuthenticationStateChanged(
      ChooseChannelRecipient(remote_device_id), secure_channel_state);
}

bool NearbyConnectionManagerImpl::DoesAuthenticatingChannelExist(
    const std::string& remote_device_id) {
  return base::Contains(remote_device_id_to_secure_channel_map_,
                        remote_device_id);
}

void NearbyConnectionManagerImpl::SetAuthenticatingChannel(
    const std::string& remote_device_id,
    std::unique_ptr<SecureChannel> secure_channel) {
  // Since a channel has been established, all connection attempts to the device
  // should be stopped. Otherwise, it would be possible to pick up additional
  // scan results and try to start a new connection. Multiple simultaneous
  // connections to the same device (e.g., one over BLE and one over Nearby) can
  // interfere with each other.
  PauseConnectionAttemptsToDevice(remote_device_id);

  if (DoesAuthenticatingChannelExist(remote_device_id)) {
    PA_LOG(ERROR) << "A new channel was created, one already exists for the "
                  << "same remote device ID. ID: "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         remote_device_id);
    NOTREACHED_IN_MIGRATION();
  }

  SecureChannel* secure_channel_raw = secure_channel.get();

  PA_LOG(INFO) << "Advertisement received; establishing connection. "
               << "Remote device ID: "
               << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                      remote_device_id);
  remote_device_id_to_secure_channel_map_[remote_device_id] =
      std::move(secure_channel);

  // Observe the channel to be notified of when either the channel authenticates
  // successfully or faces connection instability and disconnects.
  secure_channel_raw->AddObserver(this);
  secure_channel_raw->Initialize();
}

void NearbyConnectionManagerImpl::PauseConnectionAttemptsToDevice(
    const std::string& remote_device_id) {
  for (const auto& pair : GetDeviceIdPairsForRemoteDevice(remote_device_id))
    PerformCancelNearbyInitiatorConnectionAttempt(pair);
}

void NearbyConnectionManagerImpl::RestartPausedAttemptsToDevice(
    const std::string& remote_device_id) {
  for (const auto& pair : GetDeviceIdPairsForRemoteDevice(remote_device_id))
    PerformAttemptNearbyInitiatorConnection(pair);
}

void NearbyConnectionManagerImpl::ProcessPotentialLingeringChannel(
    const std::string& remote_device_id) {
  // If there was no authenticating SecureChannel associated with
  // |remote_device_id|, return early.
  if (!DoesAuthenticatingChannelExist(remote_device_id))
    return;

  // If there is at least one active request, the channel should remain active.
  if (!GetDeviceIdPairsForRemoteDevice(remote_device_id).empty())
    return;

  // Extract the map value and remove the entry from the map.
  std::unique_ptr<SecureChannel> secure_channel =
      std::move(remote_device_id_to_secure_channel_map_[remote_device_id]);
  remote_device_id_to_secure_channel_map_.erase(remote_device_id);

  // Disconnect the channel, since it is lingering with no active request.
  PA_LOG(VERBOSE)
      << "Disconnecting lingering channel which is no longer associated with "
      << "any active requests. Remote device ID: "
      << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
             remote_device_id);
  secure_channel->RemoveObserver(this);
  secure_channel_disconnector_->DisconnectSecureChannel(
      std::move(secure_channel));
}

std::string NearbyConnectionManagerImpl::GetRemoteDeviceIdForSecureChannel(
    SecureChannel* secure_channel) {
  for (const auto& map_entry : remote_device_id_to_secure_channel_map_) {
    if (map_entry.second.get() == secure_channel)
      return map_entry.first;
  }

  PA_LOG(ERROR) << "No remote device ID mapped to the provided SecureChannel.";
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

void NearbyConnectionManagerImpl::HandleSecureChannelDisconnection(
    const std::string& remote_device_id,
    bool was_authenticating) {
  if (!DoesAuthenticatingChannelExist(remote_device_id)) {
    PA_LOG(ERROR) << "HandleSecureChannelDisconnection(): Disconnected channel "
                  << "not present in map. Remote device ID: "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         remote_device_id);
    NOTREACHED_IN_MIGRATION();
  }

  for (const auto& pair : GetDeviceIdPairsForRemoteDevice(remote_device_id)) {
    NotifyNearbyInitiatorFailure(
        pair, was_authenticating
                  ? NearbyInitiatorFailureType::kAuthenticationError
                  : NearbyInitiatorFailureType::kConnectivityError);
  }

  auto it = remote_device_id_to_secure_channel_map_.find(remote_device_id);

  // It is possible that the NotifyNearbyInitiatorFailure() calls above resulted
  // in observers responding to the failure by canceling the connection attempt.
  // If all attempts to |remote_device_id| were cancelled, the disconnected
  // channel will have already been cleaned up via
  // ProcessPotentialLingeringChannel(). In that case, return early.
  if (it == remote_device_id_to_secure_channel_map_.end())
    return;

  // Stop observing the disconnected channel and remove it from the map.
  SecureChannel* secure_channel = it->second.get();
  secure_channel->RemoveObserver(this);
  remote_device_id_to_secure_channel_map_.erase(it);

  // Since the previous connection failed, the connection attempts that were
  // paused in SetAuthenticatingChannel() need to be started up again.
  RestartPausedAttemptsToDevice(remote_device_id);
}

void NearbyConnectionManagerImpl::HandleChannelAuthenticated(
    const std::string& remote_device_id) {
  // Extract the map value and remove the entry from the map.
  std::unique_ptr<SecureChannel> secure_channel =
      std::move(remote_device_id_to_secure_channel_map_[remote_device_id]);
  remote_device_id_to_secure_channel_map_.erase(remote_device_id);

  // Stop observing the channel; it is about to be passed to a client.
  secure_channel->RemoveObserver(this);

  // Before notifying clients, set |notifying_remote_device_id_|. This ensure
  // that the PerformCancel*() functions can check to see whether requests need
  // to be removed from BleScanner/BleAdvertiser.
  notifying_remote_device_id_ = remote_device_id;
  NotifyNearbyInitiatorConnectionSuccess(
      ChooseChannelRecipient(remote_device_id),
      AuthenticatedChannelImpl::Factory::Create(
          std::vector<mojom::ConnectionCreationDetail>(),
          std::move(secure_channel)));
  notifying_remote_device_id_.reset();

  // Restart any attempts which still exist.
  RestartPausedAttemptsToDevice(remote_device_id);
}

DeviceIdPair NearbyConnectionManagerImpl::ChooseChannelRecipient(
    const std::string& remote_device_id) {
  const base::flat_set<DeviceIdPair>& pairs =
      GetDeviceIdPairsForRemoteDevice(remote_device_id);
  DCHECK(!pairs.empty());
  return *pairs.begin();
}

}  // namespace ash::secure_channel
