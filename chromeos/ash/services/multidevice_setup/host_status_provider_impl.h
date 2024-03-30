// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_STATUS_PROVIDER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_STATUS_PROVIDER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/eligible_host_devices_provider.h"
#include "chromeos/ash/services/multidevice_setup/host_backend_delegate.h"
#include "chromeos/ash/services/multidevice_setup/host_status_provider.h"
#include "chromeos/ash/services/multidevice_setup/host_verifier.h"

namespace ash {

namespace multidevice_setup {

// Concrete HostStatusProvider implementation. This class listens for events
// from HostBackendDelegate, HostVerifier, and DeviceSyncClient to determine
// when the status of the host has changed.
class HostStatusProviderImpl : public HostStatusProvider,
                               public HostBackendDelegate::Observer,
                               public HostVerifier::Observer,
                               public EligibleHostDevicesProvider::Observer {
 public:
  class Factory {
   public:
    // TODO(b/320789583): Remove `device_sync_client` parameter from
    // `HostStatusProviderImpl` construction.
    // `device_sync_client` is no longer needed by the
    // `HostStatusProviderImpl` since host status is updated when
    // the `eligible_host_devices_provider` processes newly synced devices
    // and notifies `HostStatusProviderImpl` via
    // `EligibleHostDevicesProvider::Observer` to update the host status.
    static std::unique_ptr<HostStatusProvider> Create(
        EligibleHostDevicesProvider* eligible_host_devices_provider,
        HostBackendDelegate* host_backend_delegate,
        HostVerifier* host_verifier,
        device_sync::DeviceSyncClient* device_sync_client);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<HostStatusProvider> CreateInstance(
        EligibleHostDevicesProvider* eligible_host_devices_provider,
        HostBackendDelegate* host_backend_delegate,
        HostVerifier* host_verifier,
        device_sync::DeviceSyncClient* device_sync_client) = 0;

   private:
    static Factory* test_factory_;
  };

  HostStatusProviderImpl(const HostStatusProviderImpl&) = delete;
  HostStatusProviderImpl& operator=(const HostStatusProviderImpl&) = delete;

  ~HostStatusProviderImpl() override;

 private:
  HostStatusProviderImpl(
      EligibleHostDevicesProvider* eligible_host_devices_provider,
      HostBackendDelegate* host_backend_delegate,
      HostVerifier* host_verifier,
      device_sync::DeviceSyncClient* device_sync_client);

  // HostStatusProvider:
  HostStatusWithDevice GetHostWithStatus() const override;

  // HostBackendDelegate::Observer:
  void OnHostChangedOnBackend() override;
  void OnPendingHostRequestChange() override;

  // HostVerifier::Observer:
  void OnHostVerified() override;

  // EligibleHostDevicesProvider::Observer:
  void OnEligibleDevicesSynced() override;

  void CheckForUpdatedStatusAndNotifyIfChanged(
      bool force_notify_host_status_change);

  HostStatusWithDevice GetCurrentStatus();

  // Record the host status on sign-in, on status change, and every 30 minutes.
  // The latter is necessary to capture users who stay logged in for days.
  void RecordMultiDeviceHostStatus();

  raw_ptr<EligibleHostDevicesProvider> eligible_host_devices_provider_;
  raw_ptr<HostBackendDelegate> host_backend_delegate_;
  raw_ptr<HostVerifier> host_verifier_;
  HostStatusWithDevice current_status_and_device_;
  base::RepeatingTimer host_status_metric_timer_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_STATUS_PROVIDER_IMPL_H_
