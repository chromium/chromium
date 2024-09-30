// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/fwupd/fwupd_device.h"

namespace ash {

FwupdDevice::FwupdDevice() = default;

FwupdDevice::FwupdDevice(const std::string& id,
                         const std::string& device_name,
                         bool needs_reboot)
    : id(id), device_name(device_name), needs_reboot(needs_reboot) {}

FwupdDevice::FwupdDevice(const FwupdDevice& other) = default;
FwupdDevice& FwupdDevice::operator=(const FwupdDevice& other) = default;
FwupdDevice::~FwupdDevice() = default;

}  // namespace ash
