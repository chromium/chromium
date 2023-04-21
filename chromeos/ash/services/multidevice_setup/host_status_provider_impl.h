// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_STATUS_PROVIDER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_STATUS_PROVIDER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/host_backend_delegate.h"
#include "chromeos/ash/services/multidevice_setup/host_status_provider.h"
#include "chromeos/ash/services/multidevice_setup/host_verifier.h"

namespace ash {

namespace multidevice_setup {

class EligibleHostDevicesProvider;

// Concrete HostStatusProvider implementation. This class listens for events
// from HostBackendDelegate, HostVerifier, and DeviceSyncClient to determine
// when the status of the host has changed.
class HostStatusProviderImpl : public HostStatusProvider,
                               public HostBackendDelegate::Observer,
                               public HostVerifier::Observer,
                               public device_sync::DeviceSyncClient::Observer {
 public:
  class Factory {
   public:
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

  // device_sync::DeviceSyncClient::Observer:
  void OnNewDevicesSynced() override;

  void CheckForUpdatedStatusAndNotifyIfChanged(
      bool force_notify_host_status_change);

  HostStatusWithDevice GetCurrentStatus();

  // Record the host status on sign-in, on status change, and every 30 minutes.
  // The latter is necessary to capture users who stay logged in for days.
  void RecordMultiDeviceHostStatus();

  raw_ptr<EligibleHostDevicesProvider, ExperimentalAsh>
      eligible_host_devices_provider_;
  raw_ptr<HostBackendDelegate, ExperimentalAsh> host_backend_delegate_;
  raw_ptr<HostVerifier, ExperimentalAsh> host_verifier_;
  raw_ptr<device_sync::DeviceSyncClient, ExperimentalAsh> device_sync_client_;
  HostStatusWithDevice current_status_and_device_;
  base::RepeatingTimer host_status_metric_timer_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_STATUS_PROVIDER_IMPL_H_
