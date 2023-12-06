// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/multidevice/expiring_remote_device_cache.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device.h"
#include "chromeos/ash/services/device_sync/public/mojom/device_sync.mojom.h"

namespace ash {

namespace device_sync {

namespace {

bool IsValidInstanceId(const std::string& instance_id) {
  if (instance_id.empty()) {
    PA_LOG(ERROR) << "Instance ID cannot be empty.";
    return false;
  }

  std::string decoded_iid;
  if (!base::Base64UrlDecode(instance_id,
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &decoded_iid)) {
    PA_LOG(ERROR) << "Instance ID must be Base64Url encoded.";
    return false;
  }

  if (decoded_iid.size() != 8u) {
    PA_LOG(ERROR) << "Instance ID must be 8 bytes after Base64Url decoding.";
    return false;
  }

  return true;
}

}  // namespace

// static
DeviceSyncClientImpl::Factory* DeviceSyncClientImpl::Factory::test_factory_ =
    nullptr;

// static
std::unique_ptr<DeviceSyncClient> DeviceSyncClientImpl::Factory::Create() {
  if (test_factory_)
    return test_factory_->CreateInstance();

  return std::make_unique<DeviceSyncClientImpl>();
}

// static
void DeviceSyncClientImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

DeviceSyncClientImpl::Factory::~Factory() = default;

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

void DeviceSyncClientImpl::GetBetterTogetherMetadataStatus(
    mojom::DeviceSync::GetBetterTogetherMetadataStatusCallback callback) {
  device_sync_->GetBetterTogetherMetadataStatus(std::move(callback));
}

void DeviceSyncClientImpl::GetGroupPrivateKeyStatus(
    mojom::DeviceSync::GetGroupPrivateKeyStatusCallback callback) {
  device_sync_->GetGroupPrivateKeyStatus(std::move(callback));
}

multidevice::RemoteDeviceRefList DeviceSyncClientImpl::GetSyncedDevices() {
  DCHECK(is_ready());
  return expiring_device_cache_->GetNonExpiredRemoteDevices();
}

std::optional<multidevice::RemoteDeviceRef>
DeviceSyncClientImpl::GetLocalDeviceMetadata() {
  DCHECK(is_ready());
  base::UmaHistogramBoolean("CryptAuth.GetLocalDeviceMetadata.IsReady",
                            is_ready());

  // Because we expect the the client to be ready when this function is called,
  // we also expect the local device to be non-null.
  std::optional<multidevice::RemoteDeviceRef> local_device =
      expiring_device_cache_->GetRemoteDevice(local_instance_id_,
                                              local_legacy_device_id_);
  base::UmaHistogramBoolean("CryptAuth.GetLocalDeviceMetadata.Result",
                            local_device.has_value());
  if (!local_device) {
    PA_LOG(ERROR)
        << "DeviceSyncClientImpl::" << __func__
        << ": Could not retrieve local device metadata. local_instance_id="
        << local_instance_id_.value_or("[null]") << ", local_legacy_device_id="
        << local_legacy_device_id_.value_or("[null]")
        << ", is_ready=" << (is_ready() ? "yes" : "no");
  }

  return local_device;
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

void DeviceSyncClientImpl::SetFeatureStatus(
    const std::string& device_instance_id,
    multidevice::SoftwareFeature feature,
    FeatureStatusChange status_change,
    mojom::DeviceSync::SetFeatureStatusCallback callback) {
  if (!IsValidInstanceId(device_instance_id)) {
    std::move(callback).Run(mojom::NetworkRequestResult::kBadRequest);
    return;
  }

  device_sync_->SetFeatureStatus(device_instance_id, feature, status_change,
                                 std::move(callback));
}

void DeviceSyncClientImpl::FindEligibleDevices(
    multidevice::SoftwareFeature software_feature,
    FindEligibleDevicesCallback callback) {
  device_sync_->FindEligibleDevices(
      software_feature,
      base::BindOnce(&DeviceSyncClientImpl::OnFindEligibleDevicesCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}
void DeviceSyncClientImpl::NotifyDevices(
    const std::vector<std::string>& device_instance_ids,
    cryptauthv2::TargetService target_service,
    multidevice::SoftwareFeature feature,
    mojom::DeviceSync::NotifyDevicesCallback callback) {
  for (const std::string& iid : device_instance_ids) {
    if (!IsValidInstanceId(iid)) {
      std::move(callback).Run(mojom::NetworkRequestResult::kBadRequest);
      return;
    }
  }

  device_sync_->NotifyDevices(device_instance_ids, target_service, feature,
                              std::move(callback));
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
    const std::optional<std::vector<multidevice::RemoteDevice>>&
        remote_devices) {
  if (!remote_devices) {
    PA_LOG(INFO) << "Tried to fetch synced devices before service was fully "
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
    PA_LOG(INFO) << "Client is ready. Notifying new devices have synced.";
    NotifyNewDevicesSynced();
  } else {
    PA_LOG(INFO)
        << "Client is NOT ready. Waiting to notify new devices have synced.";
    pending_notify_new_synced_devices_ = true;
    AttemptToBecomeReady();
  }
}

void DeviceSyncClientImpl::OnGetLocalDeviceMetadataCompleted(
    const std::optional<multidevice::RemoteDevice>& local_device_metadata) {
  if (!local_device_metadata) {
    PA_LOG(INFO) << "Tried to get local device metadata before service was "
                    "fully initialized; waiting for enrollment to complete "
                    "before continuing.";
    return;
  }

  if (features::ShouldUseV1DeviceSync()) {
    local_instance_id_ = local_device_metadata->instance_id.empty()
                             ? std::nullopt
                             : std::make_optional<std::string>(
                                   local_device_metadata->instance_id);
    local_legacy_device_id_ = local_device_metadata->GetDeviceId().empty()
                                  ? std::nullopt
                                  : std::make_optional<std::string>(
                                        local_device_metadata->GetDeviceId());
  } else {
    local_instance_id_ = local_device_metadata->instance_id.empty()
                             ? std::nullopt
                             : std::make_optional<std::string>(
                                   local_device_metadata->instance_id);
  }

  bool has_id = local_instance_id_ || local_legacy_device_id_;
  base::UmaHistogramBoolean("CryptAuth.GetLocalDeviceMetadata.HasId", has_id);
  if (!has_id) {
    PA_LOG(ERROR) << "DeviceSyncClientImpl::" << __func__
                  << ": Local device identifiers are unexpectedly empty.";
    return;
  }

  expiring_device_cache_->UpdateRemoteDevice(*local_device_metadata);

  waiting_for_local_device_metadata_ = false;

  // Don't yet notify observers that enrollment has finished until the client
  // is ready.
  if (is_ready()) {
    PA_LOG(INFO) << "Client is ready. Notifying enrollment finished.";
    NotifyEnrollmentFinished();
  } else {
    PA_LOG(INFO)
        << "Client is NOT ready. Waiting to notify enrollment finished.";
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
    base::ranges::transform(
        response->eligible_devices, std::back_inserter(eligible_devices),
        [this](const auto& device) {
          return *expiring_device_cache_->GetRemoteDevice(device.instance_id,
                                                          device.GetDeviceId());
        });
    base::ranges::transform(response->ineligible_devices,
                            std::back_inserter(ineligible_devices),
                            [this](const auto& device) {
                              return *expiring_device_cache_->GetRemoteDevice(
                                  device.instance_id, device.GetDeviceId());
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

}  // namespace ash
