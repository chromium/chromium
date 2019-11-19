// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "chromeos/services/device_sync/public/cpp/device_sync_client_impl.h"

#include "base/bind.h"
#include "base/no_destructor.h"
#include "chromeos/components/multidevice/expiring_remote_device_cache.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/remote_device.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"

namespace chromeos {

namespace device_sync {

// static
DeviceSyncClientImpl::Factory* DeviceSyncClientImpl::Factory::test_factory_ =
    nullptr;

// static
DeviceSyncClientImpl::Factory* DeviceSyncClientImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void DeviceSyncClientImpl::Factory::SetInstanceForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

DeviceSyncClientImpl::Factory::~Factory() = default;

std::unique_ptr<DeviceSyncClient>
DeviceSyncClientImpl::Factory::BuildInstance() {
  return std::make_unique<DeviceSyncClientImpl>();
}

DeviceSyncClientImpl::DeviceSyncClientImpl()
    : expiring_device_cache_(
          std::make_unique<multidevice::ExpiringRemoteDeviceCache>()) {}

DeviceSyncClientImpl::~DeviceSyncClientImpl() = default;

void DeviceSyncClientImpl::Initialize(
    scoped_refptr<base::TaskRunner> task_runner) {
  device_sync_->AddObserver(GenerateRemote(), base::OnceClosure());

  // Delay calling these until after initialization finishes.
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&DeviceSyncClientImpl::LoadLocalDeviceMetadata,
                                weak_ptr_factory_.GetWeakPtr()));
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&DeviceSyncClientImpl::LoadSyncedDevices,
                                       weak_ptr_factory_.GetWeakPtr()));
}

mojo::Remote<mojom::DeviceSync>* DeviceSyncClientImpl::GetDeviceSyncRemote() {
  return &device_sync_;
}

void DeviceSyncClientImpl::OnEnrollmentFinished() {
  // Before notifying observers that enrollment has finished, sync down the
  // local device metadata. This ensures that observers will have access to the
  // metadata of the newly-synced local device as soon as
  // NotifyOnEnrollmentFinished() is invoked.
  LoadLocalDeviceMetadata();
}

void DeviceSyncClientImpl::OnNewDevicesSynced() {
  // Before notifying observers that new devices have synced, sync down the new
  // devices. This ensures that observers will have access to the synced devices
  // as soon as NotifyOnNewDevicesSynced() is invoked.
  LoadSyncedDevices();
}

void DeviceSyncClientImpl::ForceEnrollmentNow(
    mojom::DeviceSync::ForceEnrollmentNowCallback callback) {
  device_sync_->ForceEnrollmentNow(std::move(callback));
}

void DeviceSyncClientImpl::ForceSyncNow(
    mojom::DeviceSync::ForceSyncNowCallback callback) {
  device_sync_->ForceSyncNow(std::move(callback));
}

multidevice::RemoteDeviceRefList DeviceSyncClientImpl::GetSyncedDevices() {
  DCHECK(is_ready());
  return expiring_device_cache_->GetNonExpiredRemoteDevices();
}

base::Optional<multidevice::RemoteDeviceRef>
DeviceSyncClientImpl::GetLocalDeviceMetadata() {
  DCHECK(is_ready());
  return local_device_id_
             ? expiring_device_cache_->GetRemoteDevice(*local_device_id_)
             : base::nullopt;
}

void DeviceSyncClientImpl::SetSoftwareFeatureState(
    const std::string public_key,
    multidevice::SoftwareFeature software_feature,
    bool enabled,
    bool is_exclusive,
    mojom::DeviceSync::SetSoftwareFeatureStateCallback callback) {
  device_sync_->SetSoftwareFeatureState(public_key, software_feature, enabled,
                                        is_exclusive, std::move(callback));
}

void DeviceSyncClientImpl::FindEligibleDevices(
    multidevice::SoftwareFeature software_feature,
    FindEligibleDevicesCallback callback) {
  device_sync_->FindEligibleDevices(
      software_feature,
      base::BindOnce(&DeviceSyncClientImpl::OnFindEligibleDevicesCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DeviceSyncClientImpl::GetDevicesActivityStatus(
    mojom::DeviceSync::GetDevicesActivityStatusCallback callback) {
  device_sync_->GetDevicesActivityStatus(std::move(callback));
}

void DeviceSyncClientImpl::GetDebugInfo(
    mojom::DeviceSync::GetDebugInfoCallback callback) {
  device_sync_->GetDebugInfo(std::move(callback));
}

void DeviceSyncClientImpl::AttemptToBecomeReady() {
  if (is_ready())
    return;

  if (waiting_for_synced_devices_ || waiting_for_local_device_metadata_)
    return;

  NotifyReady();

  if (pending_notify_enrollment_finished_)
    NotifyEnrollmentFinished();

  if (pending_notify_new_synced_devices_)
    NotifyNewDevicesSynced();

  pending_notify_enrollment_finished_ = false;
  pending_notify_new_synced_devices_ = false;
}

void DeviceSyncClientImpl::LoadSyncedDevices() {
  device_sync_->GetSyncedDevices(
      base::BindOnce(&DeviceSyncClientImpl::OnGetSyncedDevicesCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceSyncClientImpl::LoadLocalDeviceMetadata() {
  device_sync_->GetLocalDeviceMetadata(
      base::BindOnce(&DeviceSyncClientImpl::OnGetLocalDeviceMetadataCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceSyncClientImpl::OnGetSyncedDevicesCompleted(
    const base::Optional<std::vector<multidevice::RemoteDevice>>&
        remote_devices) {
  if (!remote_devices) {
    PA_LOG(VERBOSE) << "Tried to fetch synced devices before service was fully "
                       "initialized; waiting for sync to complete before "
                       "continuing.";
    return;
  }

  waiting_for_synced_devices_ = false;

  if (waiting_for_local_device_metadata_)
    LoadLocalDeviceMetadata();

  expiring_device_cache_->SetRemoteDevicesAndInvalidateOldEntries(
      *remote_devices);

  // Don't yet notify observers that new devices have synced until the client
  // is ready.
  if (is_ready()) {
    NotifyNewDevicesSynced();
  } else {
    pending_notify_new_synced_devices_ = true;
    AttemptToBecomeReady();
  }
}

void DeviceSyncClientImpl::OnGetLocalDeviceMetadataCompleted(
    const base::Optional<multidevice::RemoteDevice>& local_device_metadata) {
  if (!local_device_metadata) {
    PA_LOG(VERBOSE) << "Tried to get local device metadata before service was "
                       "fully initialized; waiting for enrollment to complete "
                       "before continuing.";
    return;
  }

  local_device_id_ = local_device_metadata->GetDeviceId();
  expiring_device_cache_->UpdateRemoteDevice(*local_device_metadata);

  waiting_for_local_device_metadata_ = false;

  // Don't yet notify observers that enrollment has finished until the client
  // is ready.
  if (is_ready()) {
    NotifyEnrollmentFinished();
  } else {
    pending_notify_enrollment_finished_ = true;
    AttemptToBecomeReady();
  }
}

void DeviceSyncClientImpl::OnFindEligibleDevicesCompleted(
    FindEligibleDevicesCallback callback,
    mojom::NetworkRequestResult result_code,
    mojom::FindEligibleDevicesResponsePtr response) {
  multidevice::RemoteDeviceRefList eligible_devices;
  multidevice::RemoteDeviceRefList ineligible_devices;

  if (result_code == mojom::NetworkRequestResult::kSuccess) {
    std::transform(
        response->eligible_devices.begin(), response->eligible_devices.end(),
        std::back_inserter(eligible_devices), [this](const auto& device) {
          return *expiring_device_cache_->GetRemoteDevice(device.GetDeviceId());
        });
    std::transform(
        response->ineligible_devices.begin(),
        response->ineligible_devices.end(),
        std::back_inserter(ineligible_devices), [this](const auto& device) {
          return *expiring_device_cache_->GetRemoteDevice(device.GetDeviceId());
        });
  }

  std::move(callback).Run(result_code, eligible_devices, ineligible_devices);
}

mojo::PendingRemote<mojom::DeviceSyncObserver>
DeviceSyncClientImpl::GenerateRemote() {
  return observer_receiver_.BindNewPipeAndPassRemote();
}

void DeviceSyncClientImpl::FlushForTesting() {
  device_sync_.FlushForTesting();
}

}  // namespace device_sync

}  // namespace chromeos
