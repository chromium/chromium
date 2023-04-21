// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_HOST_SCAN_DEVICE_PRIORITIZER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_HOST_SCAN_DEVICE_PRIORITIZER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/tether/host_scan_device_prioritizer.h"

namespace ash {

namespace tether {

class TetherHostResponseRecorder;

// Implementation of HostScanDevicePrioritizer.
class HostScanDevicePrioritizerImpl : public HostScanDevicePrioritizer {
 public:
  HostScanDevicePrioritizerImpl(
      TetherHostResponseRecorder* tether_host_response_recorder);

  HostScanDevicePrioritizerImpl(const HostScanDevicePrioritizerImpl&) = delete;
  HostScanDevicePrioritizerImpl& operator=(
      const HostScanDevicePrioritizerImpl&) = delete;

  ~HostScanDevicePrioritizerImpl() override;

  // HostScanDevicePrioritizer:
  void SortByHostScanOrder(
      multidevice::RemoteDeviceRefList* remote_devices) const override;

 private:
  raw_ptr<TetherHostResponseRecorder, ExperimentalAsh>
      tether_host_response_recorder_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_HOST_SCAN_DEVICE_PRIORITIZER_IMPL_H_
