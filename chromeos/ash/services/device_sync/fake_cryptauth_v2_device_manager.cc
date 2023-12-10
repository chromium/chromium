// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/fake_cryptauth_v2_device_manager.h"

namespace ash {

namespace device_sync {

FakeCryptAuthV2DeviceManager::FakeCryptAuthV2DeviceManager() = default;

FakeCryptAuthV2DeviceManager::~FakeCryptAuthV2DeviceManager() = default;

void FakeCryptAuthV2DeviceManager::Start() {
  has_started_ = true;
}

const CryptAuthDeviceRegistry::InstanceIdToDeviceMap&
FakeCryptAuthV2DeviceManager::GetSyncedDevices() const {
  return synced_devices_;
}

void FakeCryptAuthV2DeviceManager::ForceDeviceSyncNow(
    const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
    const std::optional<std::string>& session_id) {
  DCHECK(has_started_);

  cryptauthv2::ClientMetadata client_metadata;
  client_metadata.set_invocation_reason(invocation_reason);
  if (session_id)
    client_metadata.set_session_id(*session_id);

  force_device_sync_now_requests_.push(client_metadata);

  NotifyDeviceSyncStarted(client_metadata);
}

bool FakeCryptAuthV2DeviceManager::IsDeviceSyncInProgress() const {
  return !force_device_sync_now_requests_.empty();
}

bool FakeCryptAuthV2DeviceManager::IsRecoveringFromFailure() const {
  return is_recovering_from_failure_;
}

BetterTogetherMetadataStatus
FakeCryptAuthV2DeviceManager::GetDeviceSyncerBetterTogetherMetadataStatus()
    const {
  return BetterTogetherMetadataStatus::kWaitingToProcessDeviceMetadata;
}

GroupPrivateKeyStatus
FakeCryptAuthV2DeviceManager::GetDeviceSyncerGroupPrivateKeyStatus() const {
  return GroupPrivateKeyStatus::kWaitingForGroupPrivateKey;
}

std::optional<base::Time> FakeCryptAuthV2DeviceManager::GetLastDeviceSyncTime()
    const {
  return last_device_sync_time_;
}

std::optional<base::TimeDelta>
FakeCryptAuthV2DeviceManager::GetTimeToNextAttempt() const {
  return time_to_next_attempt_;
}

void FakeCryptAuthV2DeviceManager::FinishNextForcedDeviceSync(
    const CryptAuthDeviceSyncResult& device_sync_result,
    base::Time device_sync_finish_time) {
  DCHECK(IsDeviceSyncInProgress());

  if (device_sync_result.IsSuccess()) {
    last_device_sync_time_ = device_sync_finish_time;
    is_recovering_from_failure_ = false;
  } else {
    is_recovering_from_failure_ = true;
  }

  force_device_sync_now_requests_.pop();

  NotifyDeviceSyncFinished(device_sync_result);
}

}  // namespace device_sync

}  // namespace ash
