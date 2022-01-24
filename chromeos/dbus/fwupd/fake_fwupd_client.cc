// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fwupd/fake_fwupd_client.h"

#include "chromeos/dbus/fwupd/fwupd_device.h"

#include <string>

namespace chromeos {

FakeFwupdClient::FakeFwupdClient() = default;
FakeFwupdClient::~FakeFwupdClient() = default;
void FakeFwupdClient::Init(dbus::Bus* bus) {}
void FakeFwupdClient::RequestUpgrades(std::string device_id) {}

void FakeFwupdClient::RequestDevices() {
  // TODO(swifton): This is a stub.
  auto devices = std::make_unique<FwupdDeviceList>();
  for (auto& observer : observers_)
    observer.OnDeviceListResponse(devices.get());
}

}  // namespace chromeos