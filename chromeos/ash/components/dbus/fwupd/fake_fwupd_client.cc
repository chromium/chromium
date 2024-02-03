// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "chromeos/ash/components/dbus/fwupd/fake_fwupd_client.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_device.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_update.h"

namespace {

const char kFakeDeviceIdForTesting[] = "0123";

}  // namespace

namespace ash {

FakeFwupdClient::FakeFwupdClient() = default;
FakeFwupdClient::~FakeFwupdClient() = default;
void FakeFwupdClient::Init(dbus::Bus* bus) {}

void FakeFwupdClient::RequestDevices() {
  FwupdDeviceList devices;

  // Add a fake device.
  devices.emplace_back(/*id=*/kFakeDeviceIdForTesting,
                       /*device_name=*/"fake_device");

  for (auto& observer : observers_)
    observer.OnDeviceListResponse(&devices);
}

void FakeFwupdClient::RequestUpdates(const std::string& device_id) {
  // This matches the behavior of the real class. I.e. if you send an unknown
  // id, nothing happens.
  if (device_id != kFakeDeviceIdForTesting)
    return;

  FwupdUpdateList updates;

  // Add a fake update.
  updates.emplace_back(
      /*version=*/"1.2.3", /*description=*/"Fake update description.",
      /*priority=*/1, /*filepath=*/base::FilePath("/fake/filepath"),
      /*checksum=*/"fake-checksum");

  for (auto& observer : observers_)
    observer.OnUpdateListResponse(device_id, &updates);
}

void FakeFwupdClient::InstallUpdate(const std::string& device_id,
                                    base::ScopedFD file_descriptor,
                                    FirmwareInstallOptions options) {
  // This matches the behavior of the real class. I.e. if you send an unknown
  // id, nothing happens.
  if (device_id != kFakeDeviceIdForTesting)
    return;

  for (auto& observer : observers_)
    observer.OnInstallResponse(install_success_);
}

// Implement stub method to satisfy interface.
void FakeFwupdClient::SetFwupdFeatureFlags() {}

}  // namespace ash
