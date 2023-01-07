// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/pciguard/fake_pciguard_client.h"

#include "third_party/cros_system_api/dbus/pciguard/dbus-constants.h"

namespace ash {

FakePciguardClient::FakePciguardClient() = default;
FakePciguardClient::~FakePciguardClient() = default;

// TODO(b/176763484): This is only a stub implementation. Flush this fake out
// when Pciguard has more functionality.
// Note: This function only sends a message to D-Bus and expects no returned
// value.
void FakePciguardClient::SendExternalPciDevicesPermissionState(bool permitted) {
}

void FakePciguardClient::EmitDeviceBlockedSignal(
    const std::string& device_name) {
  NotifyOnBlockedThunderboltDeviceConnected(device_name);
}

}  // namespace ash
