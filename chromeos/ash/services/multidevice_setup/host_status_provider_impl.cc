// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/host_status_provider_impl.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/multidevice_setup/eligible_host_devices_provider.h"

namespace ash {

namespace multidevice_setup {

namespace {

constexpr base::TimeDelta kHostStatusLoggingPeriod = base::Minutes(30);

}  // namespace

// static
HostStatusProviderImpl::Factory*
    HostStatusProviderImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<HostStatusProvider> HostStatusProviderImpl::Factory::Create(
    EligibleHostDevicesProvider* eligible_host_devices_provider,
    HostBackendDelegate* host_backend_delegate,
    HostVerifier* host_verifier,
    device_sync::DeviceSyncClient* device_sync_client) {
  if (test_factory_) {
    return test_factory_->CreateInstance(eligible_host_devices_provider,
                                         host_backend_delegate, host_verifier,
                                         device_sync_client);
  }

  return base::WrapUnique(new HostStatusProviderImpl(
      eligible_host_devices_provider, host_backend_delegate, host_verifier,
      device_sync_client));
}

// static
void HostStatusProviderImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

HostStatusProviderImpl::Factory::~Factory() = default;

HostStatusProviderImpl::HostStatusProviderImpl(
    EligibleHostDevicesProvider* eligible_host_devices_provider,
    HostBackendDelegate* host_backend_delegate,
    HostVerifier* host_verifier,
    device_sync::DeviceSyncClient* device_sync_client)
    : eligible_host_devices_provider_(eligible_host_devices_provider),
      host_backend_delegate_(host_backend_delegate),
      host_verifier_(host_verifier),
      current_status_and_device_(mojom::HostStatus::kNoEligibleHosts,
                                 std::nullopt /* host_device */) {
  host_backend_delegate_->AddObserver(this);
  host_verifier_->AddObserver(this);
  eligible_host_devices_provider_->AddObserver(this);

  CheckForUpdatedStatusAndNotifyIfChanged(
      /*force_notify_host_status_change=*/false);

  RecordMultiDeviceHostStatus();
  host_status_metric_timer_.Start(
      FROM_HERE, kHostStatusLoggingPeriod,
      base::BindRepeating(&HostStatusProviderImpl::RecordMultiDeviceHostStatus,
                          base::Unretained(this)));
}

HostStatusProviderImpl::~HostStatusProviderImpl() {
  host_backend_delegate_->RemoveObserver(this);
  host_verifier_->RemoveObserver(this);
  eligible_host_devices_provider_->RemoveObserver(this);
}

HostStatusProvider::HostStatusWithDevice
HostStatusProviderImpl::GetHostWithStatus() const {
  return current_status_and_device_;
}

void HostStatusProviderImpl::OnHostChangedOnBackend() {
  CheckForUpdatedStatusAndNotifyIfChanged(
      /*force_notify_host_status_change=*/false);
}

void HostStatusProviderImpl::OnPendingHostRequestChange() {
  CheckForUpdatedStatusAndNotifyIfChanged(
      /*force_notify_host_status_change=*/false);
}

void HostStatusProviderImpl::OnHostVerified() {
  CheckForUpdatedStatusAndNotifyIfChanged(
      /*force_notify_host_status_change=*/false);
}

void HostStatusProviderImpl::OnEligibleDevicesSynced() {
  CheckForUpdatedStatusAndNotifyIfChanged(
      /*force_notify_host_status_change=*/true);
}

void HostStatusProviderImpl::CheckForUpdatedStatusAndNotifyIfChanged(
    bool force_notify_host_status_change) {
  HostStatusWithDevice current_status_and_device = GetCurrentStatus();
  if (current_status_and_device == current_status_and_device_) {
    if (force_notify_host_status_change) {
      // If the RemoteDevice the host device references has changed, but not its
      // contents, fire a host status change. Note that since the status doesn't
      // actually change, neither logging nor metric collection should occur.
      NotifyHostStatusChange(current_status_and_device_.host_status(),
                             current_status_and_device_.host_device());
    }
    return;
  }

  PA_LOG(INFO) << "HostStatusProviderImpl::"
               << "CheckForUpdatedStatusAndNotifyIfChanged(): Host status "
               << "changed. New status: "
               << current_status_and_device.host_status()
               << ", Old status: " << current_status_and_device_.host_status()
               << ", Host device: "
               << (current_status_and_device.host_device()
                       ? current_status_and_device.host_device()
                             ->GetInstanceIdDeviceIdForLogs()
                       : "[no host]");

  current_status_and_device_ = current_status_and_device;
  NotifyHostStatusChange(current_status_and_device_.host_status(),
                         current_status_and_device_.host_device());
  RecordMultiDeviceHostStatus();
}

HostStatusProvider::HostStatusWithDevice
HostStatusProviderImpl::GetCurrentStatus() {
  if (host_verifier_->IsHostVerified()) {
    return HostStatusWithDevice(
        mojom::HostStatus::kHostVerified,
        *host_backend_delegate_->GetMultiDeviceHostFromBackend());
  }

  if (host_backend_delegate_->GetMultiDeviceHostFromBackend() &&
      !host_backend_delegate_->HasPendingHostRequest()) {
    return HostStatusWithDevice(
        mojom::HostStatus::kHostSetButNotYetVerified,
        *host_backend_delegate_->GetMultiDeviceHostFromBackend());
  }

  if (host_backend_delegate_->HasPendingHostRequest() &&
      host_backend_delegate_->GetPendingHostRequest()) {
    return HostStatusWithDevice(
        mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
        *host_backend_delegate_->GetPendingHostRequest());
  }

  if (!eligible_host_devices_provider_->GetEligibleHostDevices().empty()) {
    return HostStatusWithDevice(
        mojom::HostStatus::kEligibleHostExistsButNoHostSet,
        std::nullopt /* host_device */);
  }

  return HostStatusWithDevice(mojom::HostStatus::kNoEligibleHosts,
                              std::nullopt /* host_device */);
}

void HostStatusProviderImpl::RecordMultiDeviceHostStatus() {
  base::UmaHistogramEnumeration("MultiDevice.Setup.HostStatus",
                                current_status_and_device_.host_status());
}

}  // namespace multidevice_setup

}  // namespace ash
