// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/fake_host_status_provider.h"

namespace ash {

namespace multidevice_setup {

FakeHostStatusProvider::FakeHostStatusProvider() = default;

FakeHostStatusProvider::~FakeHostStatusProvider() = default;

void FakeHostStatusProvider::SetHostWithStatus(
    mojom::HostStatus host_status,
    const std::optional<multidevice::RemoteDeviceRef>& host_device) {
  bool should_notify =
      host_status_ != host_status || host_device_ != host_device;

  host_status_ = host_status;
  host_device_ = host_device;

  if (!should_notify)
    return;

  NotifyHostStatusChange(host_status_, host_device_);
}

HostStatusProvider::HostStatusWithDevice
FakeHostStatusProvider::GetHostWithStatus() const {
  return HostStatusWithDevice(host_status_, host_device_);
}

FakeHostStatusProviderObserver::FakeHostStatusProviderObserver() = default;

FakeHostStatusProviderObserver::~FakeHostStatusProviderObserver() = default;

void FakeHostStatusProviderObserver::OnHostStatusChange(
    const HostStatusProvider::HostStatusWithDevice& host_status_with_device) {
  host_status_updates_.push_back(host_status_with_device);
}

}  // namespace multidevice_setup

}  // namespace ash
