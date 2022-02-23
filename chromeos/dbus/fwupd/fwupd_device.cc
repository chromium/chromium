// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fwupd/fwupd_device.h"

namespace chromeos {

FwupdDevice::FwupdDevice() = default;

FwupdDevice::FwupdDevice(const std::string& id, const std::string& device_name)
    : id(id), device_name(device_name) {}

FwupdDevice::FwupdDevice(const FwupdDevice& other) = default;
FwupdDevice& FwupdDevice::operator=(const FwupdDevice& other) = default;
FwupdDevice::~FwupdDevice() = default;

}  // namespace chromeos
