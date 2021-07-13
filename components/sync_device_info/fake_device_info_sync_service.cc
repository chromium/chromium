// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/fake_device_info_sync_service.h"

namespace syncer {

FakeDeviceInfoSyncService::FakeDeviceInfoSyncService()
    : fake_model_type_controller_delegate_(ModelType::DEVICE_INFO) {}

FakeDeviceInfoSyncService::~FakeDeviceInfoSyncService() = default;

FakeLocalDeviceInfoProvider*
FakeDeviceInfoSyncService::GetLocalDeviceInfoProvider() {
  return &fake_local_device_info_provider_;
}

FakeDeviceInfoTracker* FakeDeviceInfoSyncService::GetDeviceInfoTracker() {
  return &fake_device_info_tracker_;
}

base::WeakPtr<ModelTypeControllerDelegate>
FakeDeviceInfoSyncService::GetControllerDelegate() {
  return fake_model_type_controller_delegate_.GetWeakPtr();
}

void FakeDeviceInfoSyncService::RefreshLocalDeviceInfo() {
  refresh_local_device_info_count_++;
}

int FakeDeviceInfoSyncService::RefreshLocalDeviceInfoCount() {
  return refresh_local_device_info_count_;
}

}  // namespace syncer
