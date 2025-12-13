// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auto_sign_out/auto_sign_out_service.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/constants/pref_names.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/local_device_info_provider.h"

namespace ash {

AutoSignOutService::AutoSignOutService(
    syncer::DeviceInfoSyncService* device_info_sync_service,
    session_manager::SessionManager* session_manager,
    PrefService* prefs)
    : device_info_sync_service_(CHECK_DEREF(device_info_sync_service)),
      session_manager_(CHECK_DEREF(session_manager)),
      prefs_(CHECK_DEREF(prefs)),
      initialization_time_(base::Time::Now()) {
  RegisterPrefListeners();
  UpdateObservations();
}

AutoSignOutService::~AutoSignOutService() = default;

void AutoSignOutService::RegisterPrefListeners() {
  pref_change_registrar_.Init(&prefs_.get());
  pref_change_registrar_.Add(
      chromeos::prefs::kAutoSignOutEnabled,
      base::BindRepeating(&AutoSignOutService::UpdateObservations,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      chromeos::prefs::kFloatingSsoEnabled,
      base::BindRepeating(&AutoSignOutService::UpdateObservations,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      chromeos::prefs::kFloatingWorkspaceV2Enabled,
      base::BindRepeating(&AutoSignOutService::UpdateObservations,
                          base::Unretained(this)));
}

void AutoSignOutService::UpdateObservations() {
  if (prefs_->GetBoolean(chromeos::prefs::kAutoSignOutEnabled) ||
      prefs_->GetBoolean(chromeos::prefs::kFloatingSsoEnabled) ||
      prefs_->GetBoolean(chromeos::prefs::kFloatingWorkspaceV2Enabled)) {
    if (!device_info_tracker_observation_.IsObserving()) {
      device_info_tracker_observation_.Observe(
          device_info_sync_service_->GetDeviceInfoTracker());
    }
    if (!session_manager_observation_.IsObserving()) {
      session_manager_observation_.Observe(&session_manager_.get());
    }
    if (!power_manager_client_observation_.IsObserving()) {
      power_manager_client_observation_.Observe(
          chromeos::PowerManagerClient::Get());
    }
    UpdateLocalDeviceInfoWhenReady();
    // When one of the relevant policies is toggled, devices might not receive
    // the policy value at the same time. One device might miss a new sign-in
    // notification from another. To fix this, manually poll for device info
    // changes.
    OnDeviceInfoChange();
  } else {
    power_manager_client_observation_.Reset();
    session_manager_observation_.Reset();
    device_info_tracker_observation_.Reset();
  }
}

void AutoSignOutService::UpdateLocalDeviceInfoWhenReady() {
  syncer::LocalDeviceInfoProvider* local_device_info_provider =
      device_info_sync_service_->GetLocalDeviceInfoProvider();

  CHECK(local_device_info_provider);

  if (local_device_info_provider->GetLocalDeviceInfo()) {
    UpdateLocalDeviceInfo();
  } else if (!local_device_info_ready_subscription_) {
    local_device_info_ready_subscription_ =
        local_device_info_provider->RegisterOnInitializedCallback(
            base::BindRepeating(
                &AutoSignOutService::OnLocalDeviceInfoProviderReady,
                weak_pointer_factory_.GetWeakPtr()));
  }
}

// TODO(crbug.com/447113190): Change the way we update DeviceInfo so that we
// don't need this PostTask.
void AutoSignOutService::OnLocalDeviceInfoProviderReady() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AutoSignOutService::UpdateLocalDeviceInfo,
                                weak_pointer_factory_.GetWeakPtr()));
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

void AutoSignOutService::OnDeviceInfoChange() {
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
      LOG(WARNING) << "An automatic sign-out is about to be performed.";
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
