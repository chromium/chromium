// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/fwupd/fake_fwupd_client.h"

#include "chromeos/ash/components/dbus/fwupd/fwupd_device.h"

#include <string>

namespace {

const char kFakeDeviceIdForTesting[] = "0123";

}  // namespace

namespace ash {

FakeFwupdClient::FakeFwupdClient() = default;
FakeFwupdClient::~FakeFwupdClient() = default;
void FakeFwupdClient::Init(dbus::Bus* bus) {}

void FakeFwupdClient::RequestDevices() {
  // TODO(swifton): This is a stub.
  FwupdDeviceList devices;
  for (auto& observer : observers_)
    observer.OnDeviceListResponse(&devices);
}

void FakeFwupdClient::RequestUpdates(const std::string& device_id) {
  // TODO(swifton): This is a stub.

  // This matches the behavior of the real class. I.e. if you send an unknown
  // id, nothing happens.
  if (device_id != kFakeDeviceIdForTesting)
    return;

  FwupdUpdateList updates;
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
