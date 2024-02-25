// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_STATUS_PROVIDER_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_STATUS_PROVIDER_H_

#include <vector>

#include "chromeos/ash/services/multidevice_setup/host_status_provider.h"

namespace ash {

namespace multidevice_setup {

// Test HostStatusProvider implementation.
class FakeHostStatusProvider : public HostStatusProvider {
 public:
  FakeHostStatusProvider();

  FakeHostStatusProvider(const FakeHostStatusProvider&) = delete;
  FakeHostStatusProvider& operator=(const FakeHostStatusProvider&) = delete;

  ~FakeHostStatusProvider() override;

  void SetHostWithStatus(
      mojom::HostStatus host_status,
      const std::optional<multidevice::RemoteDeviceRef>& host_device);

  // HostStatusProvider:
  HostStatusWithDevice GetHostWithStatus() const override;

 private:
  mojom::HostStatus host_status_ = mojom::HostStatus::kNoEligibleHosts;
  std::optional<multidevice::RemoteDeviceRef> host_device_;
};

// Test HostStatusProvider::Observer implementation.
class FakeHostStatusProviderObserver : public HostStatusProvider::Observer {
 public:
  FakeHostStatusProviderObserver();

  FakeHostStatusProviderObserver(const FakeHostStatusProviderObserver&) =
      delete;
  FakeHostStatusProviderObserver& operator=(
      const FakeHostStatusProviderObserver&) = delete;

  ~FakeHostStatusProviderObserver() override;

  const std::vector<HostStatusProvider::HostStatusWithDevice>&
  host_status_updates() const {
    return host_status_updates_;
  }

 private:
  // HostStatusProvider::Observer:
  void OnHostStatusChange(const HostStatusProvider::HostStatusWithDevice&
                              host_status_with_device) override;

  std::vector<HostStatusProvider::HostStatusWithDevice> host_status_updates_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_STATUS_PROVIDER_H_
