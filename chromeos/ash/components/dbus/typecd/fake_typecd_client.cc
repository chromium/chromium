// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/typecd/fake_typecd_client.h"
#include "third_party/cros_system_api/dbus/typecd/dbus-constants.h"

namespace ash {

FakeTypecdClient::FakeTypecdClient() = default;
FakeTypecdClient::~FakeTypecdClient() = default;

void FakeTypecdClient::EmitThunderboltDeviceConnectedSignal(
    bool is_thunderbolt_only) {
  NotifyOnThunderboltDeviceConnected(is_thunderbolt_only);
}

void FakeTypecdClient::EmitCableWarningSignal(typecd::CableWarningType type) {
  NotifyOnCableWarning(type);
}

void FakeTypecdClient::SetPeripheralDataAccessPermissionState(bool permitted) {
}

void FakeTypecdClient::SetTypeCPortsUsingDisplays(
    const std::vector<uint32_t>& port_nums) {}

}  // namespace ash
