// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auto_sign_out/auto_sign_out_service.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/local_device_info_provider.h"

namespace ash {

AutoSignOutService::AutoSignOutService(
    syncer::DeviceInfoSyncService* device_info_sync_service,
    syncer::SyncService* sync_service,
    session_manager::SessionManager* session_manager)
    : device_info_sync_service_(CHECK_DEREF(device_info_sync_service)),
      sync_service_(CHECK_DEREF(sync_service)),
      session_manager_(CHECK_DEREF(session_manager)),
      initialization_time_(base::Time::Now()) {
  sync_service_observation_.Observe(sync_service);
  session_manager_observation_.Observe(session_manager::SessionManager::Get());
  power_manager_client_observation_.Observe(
      chromeos::PowerManagerClient::Get());
  UpdateLocalDeviceInfoWhenReady();
}

AutoSignOutService::~AutoSignOutService() = default;

void AutoSignOutService::UpdateLocalDeviceInfoWhenReady() {
  syncer::LocalDeviceInfoProvider* local_device_info_provider =
      device_info_sync_service_->GetLocalDeviceInfoProvider();

  CHECK(local_device_info_provider);

  if (local_device_info_provider->GetLocalDeviceInfo()) {
    UpdateLocalDeviceInfo();
  } else {
    CHECK(!local_device_info_ready_subscription_);
    local_device_info_ready_subscription_ =
        local_device_info_provider->RegisterOnInitializedCallback(
            base::BindRepeating(&AutoSignOutService::UpdateLocalDeviceInfo,
                                weak_pointer_factory_.GetWeakPtr()));
  }
}

void AutoSignOutService::UpdateLocalDeviceInfo() {
  syncer::LocalDeviceInfoProvider* local_device_info_provider =
      device_info_sync_service_->GetLocalDeviceInfoProvider();

  syncer::MutableLocalDeviceInfoProvider* mutable_local_device_info_provider =
      static_cast<syncer::MutableLocalDeviceInfoProvider*>(
          local_device_info_provider);

  mutable_local_device_info_provider->UpdateRecentSignInTime(
      initialization_time_);
  device_info_sync_service_->RefreshLocalDeviceInfo();
}

void AutoSignOutService::OnStateChanged(syncer::SyncService* sync) {
  if (sync_service_->GetDownloadStatusFor(syncer::DataType::DEVICE_INFO) !=
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate) {
    return;
  }

  std::vector<const syncer::DeviceInfo*> all_devices =
      device_info_sync_service_->GetDeviceInfoTracker()->GetAllDeviceInfo();

  for (const syncer::DeviceInfo* device : all_devices) {
    // Skip current device info.
    if (device_info_sync_service_->GetDeviceInfoTracker()
            ->IsRecentLocalCacheGuid(device->guid())) {
      continue;
    }
    // Sign out if a device has signed in after the current device.
    if (device->auto_sign_out_last_signin_timestamp().has_value() &&
        device->auto_sign_out_last_signin_timestamp().value() >
            initialization_time_) {
      session_manager_->RequestSignOut();
      return;
    }
  }
}

void AutoSignOutService::OnUnlockScreenAttempt(
    const bool success,
    const session_manager::UnlockType unlock_type) {
  // If device is unlocked, update the device info timestamp. This is important
  // if the device was asleep, in which case we want to make sure that any other
  // devices used during sleep will sign out automatically.
  if (success) {
    UpdateLocalDeviceInfoWhenReady();
  }
}

void AutoSignOutService::SuspendDone(base::TimeDelta sleep_duration) {
  // Reset initialization time when device wakes up on the lock screen to avoid
  // unintended automatic sign-out as device might start immediately receiving
  // sync updates.
  initialization_time_ = base::Time::Now();
}

}  // namespace ash
