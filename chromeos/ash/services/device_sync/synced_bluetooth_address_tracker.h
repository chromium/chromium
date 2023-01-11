// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_SYNCED_BLUETOOTH_ADDRESS_TRACKER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_SYNCED_BLUETOOTH_ADDRESS_TRACKER_H_

#include <string>

#include "base/functional/callback.h"

namespace ash {

namespace device_sync {

// Provides a Bluetooth address to add to the encrypted metadata synced via
// DeviceSync v2, and triggers a new sync if this address has changed (e.g., if
// the user inserts a USB Bluetooth adapter).
class SyncedBluetoothAddressTracker {
 public:
  virtual ~SyncedBluetoothAddressTracker() = default;
  SyncedBluetoothAddressTracker(const SyncedBluetoothAddressTracker&) = delete;
  SyncedBluetoothAddressTracker& operator=(
      const SyncedBluetoothAddressTracker&) = delete;

  // Returns the device's Bluetooth address, formatted as a capitalized hex
  // string with colons to separate bytes (example: "01:23:45:67:89:AB"). If the
  // device does not have a Bluetooth adapter, an empty string is returned.
  using BluetoothAddressCallback = base::OnceCallback<void(const std::string&)>;
  virtual void GetBluetoothAddress(BluetoothAddressCallback callback) = 0;

  // Sets the last Bluetooth address provided during a DeviceSync attempt.
  // Should only be called after a successful sync.
  virtual void SetLastSyncedBluetoothAddress(
      const std::string& last_synced_address) = 0;

 protected:
  SyncedBluetoothAddressTracker() = default;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_SYNCED_BLUETOOTH_ADDRESS_TRACKER_H_
