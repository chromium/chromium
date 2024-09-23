// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/ble_connection_manager_impl.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/timer_factory/timer_factory.h"
#include "chromeos/ash/services/secure_channel/authenticated_channel_impl.h"
#include "chromeos/ash/services/secure_channel/ble_advertiser_impl.h"
#include "chromeos/ash/services/secure_channel/ble_initiator_failure_type.h"
#include "chromeos/ash/services/secure_channel/ble_listener_failure_type.h"
#include "chromeos/ash/services/secure_channel/ble_scanner_impl.h"
#include "chromeos/ash/services/secure_channel/ble_weave_client_connection.h"
#include "chromeos/ash/services/secure_channel/connection_metrics_logger.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/ble_constants.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/ash/services/secure_channel/secure_channel_disconnector.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace ash::secure_channel {

namespace {

std::vector<mojom::ConnectionCreationDetail> CreateConnectionDetails(
    ConnectionRole connection_role) {
  std::vector<mojom::ConnectionCreationDetail> creation_details;

  switch (connection_role) {
    case ConnectionRole::kInitiatorRole:
      creation_details.push_back(
          mojom::ConnectionCreationDetail::
              REMOTE_DEVICE_USED_FOREGROUND_BLE_ADVERTISING);
      break;
    case ConnectionRole::kListenerRole:
      creation_details.push_back(
          mojom::ConnectionCreationDetail::
              REMOTE_DEVICE_USED_BACKGROUND_BLE_ADVERTISING);
      break;
  }

  return creation_details;
}

}  // namespace

// static
BleConnectionManagerImpl::Factory*
    BleConnectionManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<BleConnectionManager> BleConnectionManagerImpl::Factory::Create(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    BluetoothHelper* bluetooth_helper,
    BleSynchronizerBase* ble_synchronizer,
    BleScanner* ble_scanner,
    SecureChannelDisconnector* secure_channel_disconnector,
    ash::timer_factory::TimerFactory* timer_factory,
    base::Clock* clock) {
  if (test_factory_) {
    return test_factory_->CreateInstance(
        bluetooth_adapter, bluetooth_helper, ble_synchronizer, ble_scanner,
        secure_channel_disconnector, timer_factory, clock);
  }

  return base::WrapUnique(new BleConnectionManagerImpl(
      bluetooth_adapter, bluetooth_helper, ble_synchronizer, ble_scanner,
      secure_channel_disconnector, timer_factory, clock));
}

// static
void BleConnectionManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

BleConnectionManagerImpl::Factory::~Factory() = default;

BleConnectionManagerImpl::ConnectionAttemptTimestamps::
    ConnectionAttemptTimestamps(ConnectionRole connection_role,
                                base::Clock* clock)
    : connection_role_(connection_role),
      clock_(clock),
      start_scan_timestamp_(clock_->Now()) {}

BleConnectionManagerImpl::ConnectionAttemptTimestamps::
    ~ConnectionAttemptTimestamps() {
  RecordEffectiveSuccessRateMetrics(false /* will_continue_to_retry */);
}

void BleConnectionManagerImpl::ConnectionAttemptTimestamps::
    RecordAdvertisementReceived() {
  DCHECK(!start_scan_timestamp_.is_null());

  if (!advertisement_received_timestamp_.is_null())
    return;
  advertisement_received_timestamp_ = clock_->Now();

  if (connection_role_ == ConnectionRole::kListenerRole) {
    LogLatencyMetric(
        "MultiDevice.SecureChannel.BLE.Performance."
        "StartScanToReceiveAdvertisementDuration.Background",
        advertisement_received_timestamp_ - start_scan_timestamp_);
  }
}

void BleConnectionManagerImpl::ConnectionAttemptTimestamps::
    RecordGattConnectionEstablished() {
  DCHECK(!start_scan_timestamp_.is_null());
  DCHECK(!advertisement_received_timestamp_.is_null());

  if (!gatt_connection_timestamp_.is_null())
    return;
  gatt_connection_timestamp_ = clock_->Now();

  if (connection_role_ == ConnectionRole::kListenerRole) {
    LogLatencyMetric(
        "MultiDevice.SecureChannel.BLE.Performance."
        "ReceiveAdvertisementToConnectionDuration.Background",
        gatt_connection_timestamp_ - advertisement_received_timestamp_);
    LogLatencyMetric(
        "MultiDevice.SecureChannel.BLE.Performance."
        "StartScanToConnectionDuration.Background",
        gatt_connection_timestamp_ - start_scan_timestamp_);
  }
}

void BleConnectionManagerImpl::ConnectionAttemptTimestamps::
    RecordChannelAuthenticated() {
  DCHECK(!start_scan_timestamp_.is_null());
  DCHECK(!advertisement_received_timestamp_.is_null());
  DCHECK(!gatt_connection_timestamp_.is_null());

  if (!authentication_timestamp_.is_null())
    return;
  authentication_timestamp_ = clock_->Now();

  if (connection_role_ == ConnectionRole::kListenerRole) {
    LogLatencyMetric(
        "MultiDevice.SecureChannel.BLE.Performance."
        "ConnectionToAuthenticationDuration.Background",
        authentication_timestamp_ - gatt_connection_timestamp_);
  }
}

void BleConnectionManagerImpl::ConnectionAttemptTimestamps::Reset() {
  RecordEffectiveSuccessRateMetrics(true /* will_continue_to_retry */);

  start_scan_timestamp_ = clock_->Now();
  advertisement_received_timestamp_ = base::Time();
  gatt_connection_timestamp_ = base::Time();
  authentication_timestamp_ = base::Time();
}

void BleConnectionManagerImpl::ConnectionAttemptTimestamps::
    RecordEffectiveSuccessRateMetrics(bool will_continue_to_retry) {
  bool has_received_advertisement =
      !advertisement_received_timestamp_.is_null();
  bool has_established_gatt_connection = !gatt_connection_timestamp_.is_null();
  bool has_authenticated = !authentication_timestamp_.is_null();

  // Received advertisement ==> GATT connection effective success rate:
  // (a) Log "success" if a GATT connection was established.
  // (b) Log "fail" if an advertisement was received but no GATT connection was
  //     established, but only if there are no more retries.
  // (c) Log nothing at all if no advertisement was received.
  if (has_established_gatt_connection ||
      (!will_continue_to_retry && has_received_advertisement)) {
    UMA_HISTOGRAM_BOOLEAN(
        "MultiDevice.SecureChannel.BLE.ReceiveAdvertisementToGattConnection."
        "EffectiveSuccessRateWithRetries",
        has_established_gatt_connection);
  }

  // Received advertisement ==> authentication effective success rate:
  // (a) Log "success" if authentication succeeded.
  // (b) Log "fail" if an advertisement was received but no authentication,
  //     occurred, but only if there are no more retries.
  // (c) Log nothing at all if no advertisement was received.
  if (has_authenticated ||
      (!will_continue_to_retry && has_received_advertisement)) {
    UMA_HISTOGRAM_BOOLEAN(
        "MultiDevice.SecureChannel.BLE.ReceiveAdvertisementToAuthentication."
        "EffectiveSuccessRateWithRetries",
        has_authenticated);
  }

  // GATT connection ==> authentication effective success rate:
  // (a) Log "success" if authentication succeeded.
  // (b) Log "fail" if a GATT connection was established but no authentication,
  //     occurred, but only if there are no more retries.
  // (c) Log nothing at all if no GATT connection was established.
  if (has_authenticated ||
      (!will_continue_to_retry && has_established_gatt_connection)) {
    UMA_HISTOGRAM_BOOLEAN(
        "MultiDevice.SecureChannel.BLE.GattConnectionToAuthentication."
        "EffectiveSuccessRateWithRetries",
        has_authenticated);
  }
}

BleConnectionManagerImpl::BleConnectionManagerImpl(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    BluetoothHelper* bluetooth_helper,
    BleSynchronizerBase* ble_synchronizer,
    BleScanner* ble_scanner,
    SecureChannelDisconnector* secure_channel_disconnector,
    ash::timer_factory::TimerFactory* timer_factory,
    base::Clock* clock)
    : bluetooth_adapter_(bluetooth_adapter),
      clock_(clock),
      ble_scanner_(ble_scanner),
      secure_channel_disconnector_(secure_channel_disconnector),
      ble_advertiser_(BleAdvertiserImpl::Factory::Create(this /* delegate */,
                                                         bluetooth_helper,
                                                         ble_synchronizer,
                                                         timer_factory)) {
  ble_scanner_->AddObserver(this);
}

BleConnectionManagerImpl::~BleConnectionManagerImpl() {
  ble_scanner_->RemoveObserver(this);
}

void BleConnectionManagerImpl::PerformAttemptBleInitiatorConnection(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority) {
  if (DoesAuthenticatingChannelExist(device_id_pair.remote_device_id()))
    return;

  StartConnectionAttemptTimerMetricsIfNecessary(
      device_id_pair.remote_device_id(), ConnectionRole::kInitiatorRole);

  ble_advertiser_->AddAdvertisementRequest(device_id_pair, connection_priority);
  ble_scanner_->AddScanRequest(ConnectionAttemptDetails(
      device_id_pair, ConnectionMedium::kBluetoothLowEnergy,
      ConnectionRole::kInitiatorRole));
}

void BleConnectionManagerImpl::PerformUpdateBleInitiatorConnectionPriority(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority) {
  if (DoesAuthenticatingChannelExist(device_id_pair.remote_device_id()))
    return;

  ble_advertiser_->UpdateAdvertisementRequestPriority(device_id_pair,
                                                      connection_priority);
}

void BleConnectionManagerImpl::PerformCancelBleInitiatorConnectionAttempt(
    const DeviceIdPair& device_id_pair) {
  RemoveConnectionAttemptTimerMetricsIfNecessary(
      device_id_pair.remote_device_id());

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

  ble_advertiser_->RemoveAdvertisementRequest(device_id_pair);
  ble_scanner_->RemoveScanRequest(ConnectionAttemptDetails(
      device_id_pair, ConnectionMedium::kBluetoothLowEnergy,
      ConnectionRole::kInitiatorRole));
}

void BleConnectionManagerImpl::PerformAttemptBleListenerConnection(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority) {
  if (DoesAuthenticatingChannelExist(device_id_pair.remote_device_id()))
    return;

  StartConnectionAttemptTimerMetricsIfNecessary(
      device_id_pair.remote_device_id(), ConnectionRole::kListenerRole);

  ble_scanner_->AddScanRequest(ConnectionAttemptDetails(
      device_id_pair, ConnectionMedium::kBluetoothLowEnergy,
      ConnectionRole::kListenerRole));
}

void BleConnectionManagerImpl::PerformUpdateBleListenerConnectionPriority(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority) {
  // BLE scans are not prioritized, so nothing needs to be done.
}

void BleConnectionManagerImpl::PerformCancelBleListenerConnectionAttempt(
    const DeviceIdPair& device_id_pair) {
  RemoveConnectionAttemptTimerMetricsIfNecessary(
      device_id_pair.remote_device_id());

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
      device_id_pair, ConnectionMedium::kBluetoothLowEnergy,
      ConnectionRole::kListenerRole));
}

void BleConnectionManagerImpl::OnAdvertisingSlotEnded(
    const DeviceIdPair& device_id_pair,
    bool replaced_by_higher_priority_advertisement) {
  NotifyBleInitiatorFailure(
      device_id_pair,
      replaced_by_higher_priority_advertisement
          ? BleInitiatorFailureType::
                kInterruptedByHigherPriorityConnectionAttempt
          : BleInitiatorFailureType::kTimeoutContactingRemoteDevice);
}

void BleConnectionManagerImpl::OnFailureToGenerateAdvertisement(
    const DeviceIdPair& device_id_pair) {
  NotifyBleInitiatorFailure(
      device_id_pair, BleInitiatorFailureType::kCouldNotGenerateAdvertisement);
}

void BleConnectionManagerImpl::OnReceivedAdvertisement(
    multidevice::RemoteDeviceRef remote_device,
    device::BluetoothDevice* bluetooth_device,
    ConnectionMedium connection_medium,
    ConnectionRole connection_role,
    const std::vector<uint8_t>& eid) {
  // Only process advertisements received as part of the BLE connection flow.
  if (connection_medium != ConnectionMedium::kBluetoothLowEnergy)
    return;

  remote_device_id_to_timestamps_map_[remote_device.GetDeviceId()]
      ->RecordAdvertisementReceived();

  // Create a connection to the device.
  std::unique_ptr<Connection> connection =
      weave::BluetoothLowEnergyWeaveClientConnection::Factory::Create(
          remote_device, bluetooth_adapter_,
          device::BluetoothUUID(kGattServerUuid),
          bluetooth_device->GetAddress(),
          true /* should_set_low_connection_latency */);

  SetAuthenticatingChannel(
      remote_device.GetDeviceId(),
      SecureChannel::Factory::Create(std::move(connection)), connection_role);
}

void BleConnectionManagerImpl::OnSecureChannelStatusChanged(
    SecureChannel* secure_channel,
    const SecureChannel::Status& old_status,
    const SecureChannel::Status& new_status) {
  std::string remote_device_id =
      GetRemoteDeviceIdForSecureChannel(secure_channel);

  if (new_status == SecureChannel::Status::DISCONNECTED) {
    HandleSecureChannelDisconnection(
        remote_device_id, old_status == SecureChannel::Status::AUTHENTICATING
        /* was_authenticating */);
    return;
  }

  if (new_status == SecureChannel::Status::CONNECTED) {
    remote_device_id_to_timestamps_map_[remote_device_id]
        ->RecordGattConnectionEstablished();
  }

  if (new_status == SecureChannel::Status::AUTHENTICATED) {
    HandleChannelAuthenticated(remote_device_id);
    return;
  }
}

bool BleConnectionManagerImpl::DoesAuthenticatingChannelExist(
    const std::string& remote_device_id) {
  return base::Contains(remote_device_id_to_secure_channel_map_,
                        remote_device_id);
}

void BleConnectionManagerImpl::SetAuthenticatingChannel(
    const std::string& remote_device_id,
    std::unique_ptr<SecureChannel> secure_channel,
    ConnectionRole connection_role) {
  // Since a channel has been established, all connection attempts to the device
  // should be stopped. Otherwise, it would be possible to pick up additional
  // scan results and try to start a new connection. Multiple simultaneous BLE
  // connections to the same device can interfere with each other.
  PauseConnectionAttemptsToDevice(remote_device_id);

  if (DoesAuthenticatingChannelExist(remote_device_id)) {
    PA_LOG(ERROR) << "BleConnectionManager::SetAuthenticatingChannel(): A new "
                  << "channel was created, one already exists for the same "
                  << "remote device ID. ID: "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         remote_device_id);
    NOTREACHED_IN_MIGRATION();
  }

  SecureChannel* secure_channel_raw = secure_channel.get();

  PA_LOG(INFO) << "BleConnectionManager::SetAuthenticatingChannel(): "
               << "Advertisement received; establishing connection. "
               << "Remote device ID: "
               << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                      remote_device_id)
               << ", Connection role: " << connection_role;
  remote_device_id_to_secure_channel_map_[remote_device_id] =
      std::make_pair(std::move(secure_channel), connection_role);

  // Observe the channel to be notified of when either the channel authenticates
  // successfully or faces BLE instability and disconnects.
  secure_channel_raw->AddObserver(this);
  secure_channel_raw->Initialize();
}

void BleConnectionManagerImpl::PauseConnectionAttemptsToDevice(
    const std::string& remote_device_id) {
  for (const auto& details : GetDetailsForRemoteDevice(remote_device_id)) {
    switch (details.connection_role()) {
      case ConnectionRole::kInitiatorRole:
        PerformCancelBleInitiatorConnectionAttempt(details.device_id_pair());
        break;
      case ConnectionRole::kListenerRole:
        PerformCancelBleListenerConnectionAttempt(details.device_id_pair());
        break;
    }
  }
}

void BleConnectionManagerImpl::RestartPausedAttemptsToDevice(
    const std::string& remote_device_id) {
  for (const auto& details : GetDetailsForRemoteDevice(remote_device_id)) {
    ConnectionPriority connection_priority = GetPriorityForAttempt(
        details.device_id_pair(), details.connection_role());

    switch (details.connection_role()) {
      case ConnectionRole::kInitiatorRole:
        PerformAttemptBleInitiatorConnection(details.device_id_pair(),
                                             connection_priority);
        break;
      case ConnectionRole::kListenerRole:
        PerformAttemptBleListenerConnection(details.device_id_pair(),
                                            connection_priority);
        break;
    }
  }
}

void BleConnectionManagerImpl::ProcessPotentialLingeringChannel(
    const std::string& remote_device_id) {
  // If there was no authenticating SecureChannel associated with
  // |remote_device_id|, return early.
  if (!DoesAuthenticatingChannelExist(remote_device_id))
    return;

  // If there is at least one active request, the channel should remain active.
  if (!GetDetailsForRemoteDevice(remote_device_id).empty())
    return;

  // Extract the map value and remove the entry from the map.
  SecureChannelWithRole channel_with_role =
      std::move(remote_device_id_to_secure_channel_map_[remote_device_id]);
  remote_device_id_to_secure_channel_map_.erase(remote_device_id);

  // Disconnect the channel, since it is lingering with no active request.
  PA_LOG(VERBOSE)
      << "BleConnectionManagerImpl::"
      << "ProcessPotentialLingeringChannel(): Disconnecting lingering "
      << "channel which is no longer associated with any active "
      << "requests. Remote device ID: "
      << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
             remote_device_id);
  channel_with_role.first->RemoveObserver(this);
  secure_channel_disconnector_->DisconnectSecureChannel(
      std::move(channel_with_role.first));
}

std::string BleConnectionManagerImpl::GetRemoteDeviceIdForSecureChannel(
    SecureChannel* secure_channel) {
  for (const auto& map_entry : remote_device_id_to_secure_channel_map_) {
    if (map_entry.second.first.get() == secure_channel)
      return map_entry.first;
  }

  PA_LOG(ERROR) << "BleConnectionManager::GetRemoteDeviceIdForSecureChannel(): "
                << "No remote device ID mapped to the provided SecureChannel. ";
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

void BleConnectionManagerImpl::HandleSecureChannelDisconnection(
    const std::string& remote_device_id,
    bool was_authenticating) {
  // Since the channel has disconnected, the timestamps used to track the
  // connection's latency should be reset and started anew.
  remote_device_id_to_timestamps_map_[remote_device_id]->Reset();

  if (!DoesAuthenticatingChannelExist(remote_device_id)) {
    PA_LOG(ERROR) << "BleConnectionManagerImpl::"
                  << "HandleSecureChannelDisconnection(): Disconnected channel "
                  << "not present in map. Remote device ID: "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         remote_device_id);
    NOTREACHED_IN_MIGRATION();
  }

  for (const auto& details : GetDetailsForRemoteDevice(remote_device_id)) {
    switch (details.connection_role()) {
      // Initiator role devices are notified of authentication errors as well as
      // GATT instability errors.
      case ConnectionRole::kInitiatorRole:
        NotifyBleInitiatorFailure(
            details.device_id_pair(),
            was_authenticating ? BleInitiatorFailureType::kAuthenticationError
                               : BleInitiatorFailureType::kGattConnectionError);
        break;

      // Listener role devices are only notified of authentication errors.
      case ConnectionRole::kListenerRole:
        if (was_authenticating) {
          NotifyBleListenerFailure(
              details.device_id_pair(),
              BleListenerFailureType::kAuthenticationError);
        }
        break;
    }
  }

  // It is possible that the NotifyBle*Failure() calls above resulted in
  // observers responding to the failure by canceling the connection attempt.
  // If all attempts to |remote_device_id| were cancelled, the disconnected
  // channel will have already been cleaned up via
  // ProcessPotentialLingeringChannel().
  auto it = remote_device_id_to_secure_channel_map_.find(remote_device_id);
  if (it == remote_device_id_to_secure_channel_map_.end())
    return;

  // Stop observing the disconnected channel and remove it from the map.
  SecureChannelWithRole& secure_channel_with_role = it->second;
  secure_channel_with_role.first->RemoveObserver(this);
  remote_device_id_to_secure_channel_map_.erase(it);

  // Since the previous connection failed, the connection attempts that were
  // paused in SetAuthenticatingChannel() need to be started up again. Note
  // that it is possible that clients handled being notified of the GATT failure
  // above by removing the connection request due to too many failures.
  RestartPausedAttemptsToDevice(remote_device_id);
}

void BleConnectionManagerImpl::HandleChannelAuthenticated(
    const std::string& remote_device_id) {
  remote_device_id_to_timestamps_map_[remote_device_id]
      ->RecordChannelAuthenticated();
  remote_device_id_to_timestamps_map_.erase(remote_device_id);

  // Extract the map value and remove the entry from the map.
  SecureChannelWithRole channel_with_role =
      std::move(remote_device_id_to_secure_channel_map_[remote_device_id]);
  remote_device_id_to_secure_channel_map_.erase(remote_device_id);

  // Stop observing the channel; it is about to be passed to a client.
  channel_with_role.first->RemoveObserver(this);

  ConnectionAttemptDetails channel_to_receive =
      ChooseChannelRecipient(remote_device_id, channel_with_role.second);

  // Before notifying clients, set |notifying_remote_device_id_|. This ensure
  // that the PerformCancel*() functions can check to see whether requests need
  // to be removed from BleScanner/BleAdvertiser.
  notifying_remote_device_id_ = remote_device_id;
  NotifyConnectionSuccess(channel_to_receive.device_id_pair(),
                          channel_to_receive.connection_role(),
                          AuthenticatedChannelImpl::Factory::Create(
                              CreateConnectionDetails(channel_with_role.second),
                              std::move(channel_with_role.first)));
  notifying_remote_device_id_.reset();

  // Restart any attempts which still exist.
  RestartPausedAttemptsToDevice(remote_device_id);
}

ConnectionAttemptDetails BleConnectionManagerImpl::ChooseChannelRecipient(
    const std::string& remote_device_id,
    ConnectionRole connection_role) {
  // More than one connection attempt could correspond to this channel. If so,
  // arbitrarily choose the first one as the recipient of the authenticated
  // channel.
  for (const auto& details : GetDetailsForRemoteDevice(remote_device_id)) {
    // Initiator role corresponds to foreground advertisements.
    if (details.connection_role() == ConnectionRole::kInitiatorRole &&
        connection_role == ConnectionRole::kInitiatorRole) {
      return details;
    }

    // Listener role corresponds to background advertisements.
    if (details.connection_role() == ConnectionRole::kListenerRole &&
        connection_role == ConnectionRole::kListenerRole) {
      return details;
    }
  }

  PA_LOG(ERROR) << "BleConnectionManager::ChooseChannelRecipient(): Could not "
                << "find DeviceIdPair to receive channel. Remote device ID: "
                << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                       remote_device_id)
                << ", Role: " << connection_role;
  NOTREACHED_IN_MIGRATION();
  return ConnectionAttemptDetails(std::string(), std::string(),
                                  ConnectionMedium::kBluetoothLowEnergy,
                                  ConnectionRole::kInitiatorRole);
}

void BleConnectionManagerImpl::StartConnectionAttemptTimerMetricsIfNecessary(
    const std::string& remote_device_id,
    ConnectionRole connection_role) {
  // If an entry already exists, there is nothing to do. This is expected if
  // more than one client is attempting a connection to the same device.
  if (base::Contains(remote_device_id_to_timestamps_map_, remote_device_id))
    return;

  remote_device_id_to_timestamps_map_[remote_device_id] =
      std::make_unique<ConnectionAttemptTimestamps>(connection_role, clock_);
}

void BleConnectionManagerImpl::RemoveConnectionAttemptTimerMetricsIfNecessary(
    const std::string& remote_device_id) {
  // If there is at least one active request, latency metrics should continue
  // tracking the connection attempt.
  if (!GetDetailsForRemoteDevice(remote_device_id).empty())
    return;

  remote_device_id_to_timestamps_map_.erase(remote_device_id);
}

}  // namespace ash::secure_channel
