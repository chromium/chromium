// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/host_status_provider.h"

#include "base/logging.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash {

namespace multidevice_setup {

namespace {

std::string HostStatusToString(mojom::HostStatus status) {
  switch (status) {
    case mojom::HostStatus::kNoEligibleHosts:
      return "[kNoEligibleHosts]";
    case mojom::HostStatus::kEligibleHostExistsButNoHostSet:
      return "[kEligibleHostExistsButNoHostSet]";
    case mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation:
      return "[kHostSetLocallyButWaitingForBackendConfirmation]";
    case mojom::HostStatus::kHostSetButNotYetVerified:
      return "[kHostSetButNotYetVerified]";
    case mojom::HostStatus::kHostVerified:
      return "[kHostVerified]";
  }
}

std::string HostStatusWithDeviceToString(
    mojom::HostStatus host_status,
    const std::optional<multidevice::RemoteDeviceRef>& host_device) {
  std::ostringstream stream;
  stream << "{" << std::endl;
  stream << "  " << HostStatusToString(host_status) << ": "
         << (host_device ? host_device->pii_free_name() : " no device")
         << std::endl;
  stream << "}";
  return stream.str();
}

}  // namespace

HostStatusProvider::HostStatusWithDevice::HostStatusWithDevice(
    mojom::HostStatus host_status,
    const std::optional<multidevice::RemoteDeviceRef>& host_device)
    : host_status_(host_status), host_device_(host_device) {
  if (host_status_ == mojom::HostStatus::kNoEligibleHosts ||
      host_status_ == mojom::HostStatus::kEligibleHostExistsButNoHostSet) {
    if (host_device_) {
      PA_LOG(ERROR) << "HostStatusWithDevice::HostStatusWithDevice(): Tried to "
                    << "construct a HostStatusWithDevice with a status "
                    << "indicating no device, but a device was provided. "
                    << "Status: " << host_status_ << ", IDs: "
                    << host_device_->GetInstanceIdDeviceIdForLogs();
      NOTREACHED_IN_MIGRATION();
    }
  } else if (!host_device_) {
    PA_LOG(ERROR) << "HostStatusWithDevice::HostStatusWithDevice(): Tried to "
                  << "construct a HostStatusWithDevice with a status "
                  << "indicating a device, but no device was provided. "
                  << "Status: " << host_status_;
    NOTREACHED_IN_MIGRATION();
  }
}

HostStatusProvider::HostStatusWithDevice::HostStatusWithDevice(
    const HostStatusWithDevice& other) = default;

HostStatusProvider::HostStatusWithDevice::~HostStatusWithDevice() = default;

bool HostStatusProvider::HostStatusWithDevice::operator==(
    const HostStatusWithDevice& other) const {
  return host_status_ == other.host_status_ &&
         host_device_ == other.host_device_;
}

bool HostStatusProvider::HostStatusWithDevice::operator!=(
    const HostStatusWithDevice& other) const {
  return !(*this == other);
}

HostStatusProvider::HostStatusProvider() = default;

HostStatusProvider::~HostStatusProvider() = default;

void HostStatusProvider::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void HostStatusProvider::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void HostStatusProvider::NotifyHostStatusChange(
    mojom::HostStatus host_status,
    const std::optional<multidevice::RemoteDeviceRef>& host_device) {
  HostStatusWithDevice host_status_with_device(host_status, host_device);
  PA_LOG(INFO) << __func__ << ": "
               << HostStatusWithDeviceToString(host_status, host_device);
  for (auto& observer : observer_list_)
    observer.OnHostStatusChange(host_status_with_device);
}

}  // namespace multidevice_setup

}  // namespace ash
