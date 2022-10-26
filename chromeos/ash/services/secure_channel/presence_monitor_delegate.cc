// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/presence_monitor_delegate.h"

#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device.h"
#include "chromeos/ash/components/multidevice/remote_device_cache.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/secure_channel/ble_scanner_impl.h"
#include "chromeos/ash/services/secure_channel/ble_synchronizer.h"
#include "chromeos/ash/services/secure_channel/bluetooth_helper_impl.h"
#include "chromeos/ash/services/secure_channel/connection_role.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash::secure_channel {

PresenceMonitorDelegate::PresenceMonitorDelegate(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    PresenceMonitor::DeviceSeenCallback device_seen_callback)
    : bluetooth_adapter_(bluetooth_adapter),
      device_seen_callback_(std::move(device_seen_callback)),
      remote_device_cache_(multidevice::RemoteDeviceCache::Factory::Create()),
      bluetooth_helper_(
          BluetoothHelperImpl::Factory::Create(remote_device_cache_.get())),
      ble_synchronizer_(BleSynchronizer::Factory::Create(bluetooth_adapter_)),
      ble_scanner_(BleScannerImpl::Factory::Create(bluetooth_helper_.get(),
                                                   ble_synchronizer_.get(),
                                                   bluetooth_adapter_)) {
  ble_scanner_->AddObserver(this);
}

PresenceMonitorDelegate::~PresenceMonitorDelegate() {
  ble_scanner_->RemoveObserver(this);
}

void PresenceMonitorDelegate::StartMonitoring(
    const multidevice::RemoteDevice& remote_device,
    const multidevice::RemoteDevice& local_device) {
  PA_LOG(INFO) << "Starting monitoring proximity";
  remote_device_id_ = remote_device.GetDeviceId();
  local_device_id_ = local_device.GetDeviceId();

  remote_device_cache_->SetRemoteDevices({remote_device, local_device});

  ble_scanner_->AddScanRequest(ConnectionAttemptDetails(
      remote_device_id_, local_device_id_,
      ConnectionMedium::kBluetoothLowEnergy, ConnectionRole::kListenerRole));
}

void PresenceMonitorDelegate::StopMonitoring() {
  PA_LOG(INFO) << "Stopping monitoring proximity";
  ble_scanner_->RemoveScanRequest(ConnectionAttemptDetails(
      remote_device_id_, local_device_id_,
      ConnectionMedium::kBluetoothLowEnergy, ConnectionRole::kListenerRole));
  remote_device_id_.clear();
  local_device_id_.clear();
}

void PresenceMonitorDelegate::OnReceivedAdvertisement(
    multidevice::RemoteDeviceRef remote_device_ref,
    device::BluetoothDevice* bluetooth_device,
    ConnectionMedium connection_medium,
    ConnectionRole connection_role,
    const std::vector<uint8_t>& eid) {
  // It is the responsibility of the scanner to ensure outdated advertisements
  // are not forwarded through, so we will treat all received advertisements as
  // valid.
  device_seen_callback_.Run();
}

}  // namespace ash::secure_channel
