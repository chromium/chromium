// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/fake_eligible_host_devices_provider.h"

namespace ash {

namespace multidevice_setup {

FakeEligibleHostDevicesProvider::FakeEligibleHostDevicesProvider() = default;

FakeEligibleHostDevicesProvider::~FakeEligibleHostDevicesProvider() = default;

multidevice::RemoteDeviceRefList
FakeEligibleHostDevicesProvider::GetEligibleHostDevices() const {
  return eligible_host_devices_;
}

multidevice::DeviceWithConnectivityStatusList
FakeEligibleHostDevicesProvider::GetEligibleActiveHostDevices() const {
  return eligible_active_host_devices_;
}

}  // namespace multidevice_setup

}  // namespace ash
