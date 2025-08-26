// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auto_sign_out/auto_sign_out_service.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/local_device_info_provider.h"

namespace ash {

AutoSignOutService::AutoSignOutService(
    syncer::DeviceInfoSyncService* device_info_sync_service)
    : device_info_sync_service_(CHECK_DEREF(device_info_sync_service)),
      initialization_time_(base::Time::Now()) {
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

AutoSignOutService::~AutoSignOutService() = default;

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

}  // namespace ash
