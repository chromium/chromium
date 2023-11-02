// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_HOST_SCAN_DEVICE_PRIORITIZER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_HOST_SCAN_DEVICE_PRIORITIZER_H_

#include "chromeos/ash/components/multidevice/remote_device_ref.h"

namespace ash {

namespace tether {

// Prioritizes the order of devices when performing a host scan. To optimize for
// the most common tethering operations, this class uses the following rules:
//   * The device which has most recently sent a successful
//     ConnectTetheringResponse is always at the front of the queue.
//   * Devices which have most recently sent a successful
//     TetherAvailabilityResponse are next in the order, as long as they do not
//     violate the first rule.
//   * All other devices are left in the order they are passed.
class HostScanDevicePrioritizer {
 public:
  HostScanDevicePrioritizer() {}

  HostScanDevicePrioritizer(const HostScanDevicePrioritizer&) = delete;
  HostScanDevicePrioritizer& operator=(const HostScanDevicePrioritizer&) =
      delete;

  virtual ~HostScanDevicePrioritizer() {}

  // Prioritizes |remote_devices| using the rules described above.
  virtual void SortByHostScanOrder(
      multidevice::RemoteDeviceRefList* remote_devices) const = 0;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_HOST_SCAN_DEVICE_PRIORITIZER_H_
