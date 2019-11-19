// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_scanner.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chromeos/components/multidevice/logging/logging.h"

namespace chromeos {

namespace secure_channel {

BleScanner::BleScanner(Delegate* delegate) : delegate_(delegate) {}

BleScanner::~BleScanner() = default;

void BleScanner::AddScanFilter(const ScanFilter& scan_filter) {
  if (base::Contains(scan_filters_, scan_filter)) {
    PA_LOG(ERROR) << "BleScanner::AddScanFilter(): Tried to add a scan filter "
                  << "which already existed. Filter: " << scan_filter;
    NOTREACHED();
  }

  scan_filters_.insert(scan_filter);
  HandleScanFilterChange();
}

void BleScanner::RemoveScanFilter(const ScanFilter& scan_filter) {
  if (!base::Contains(scan_filters_, scan_filter)) {
    PA_LOG(ERROR) << "BleScanner::RemoveScanFilter(): Tried to remove a scan "
                  << "filter which was not present. Filter: " << scan_filter;
    NOTREACHED();
  }

  scan_filters_.erase(scan_filter);
  HandleScanFilterChange();
}

bool BleScanner::HasScanFilter(const ScanFilter& scan_filter) {
  return base::Contains(scan_filters_, scan_filter);
}

DeviceIdPairSet BleScanner::GetAllDeviceIdPairs() {
  DeviceIdPairSet set;
  for (const auto& scan_filter : scan_filters_)
    set.insert(scan_filter.first);
  return set;
}

void BleScanner::NotifyReceivedAdvertisementFromDevice(
    const multidevice::RemoteDeviceRef& remote_device,
    device::BluetoothDevice* bluetooth_device,
    ConnectionRole connection_role) {
  delegate_->OnReceivedAdvertisement(remote_device, bluetooth_device,
                                     connection_role);
}

std::ostream& operator<<(std::ostream& stream,
                         const BleScanner::ScanFilter& scan_filter) {
  stream << "{device_id_pair: " << scan_filter.first
         << ", connection_role: " << scan_filter.second << "}";
  return stream;
}

}  // namespace secure_channel

}  // namespace chromeos
