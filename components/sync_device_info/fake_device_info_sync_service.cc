// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/fake_device_info_sync_service.h"

namespace syncer {

FakeDeviceInfoSyncService::FakeDeviceInfoSyncService(
    bool skip_engine_connection)
    : fake_data_type_controller_delegate_(DataType::DEVICE_INFO) {
  if (skip_engine_connection) {
    fake_data_type_controller_delegate_
        .EnableSkipEngineConnectionForActivationResponse();
  }
}

FakeDeviceInfoSyncService::~FakeDeviceInfoSyncService() = default;

FakeLocalDeviceInfoProvider*
FakeDeviceInfoSyncService::GetLocalDeviceInfoProvider() {
  return &fake_local_device_info_provider_;
}

FakeDeviceInfoTracker* FakeDeviceInfoSyncService::GetDeviceInfoTracker() {
  return &fake_device_info_tracker_;
}

base::WeakPtr<DataTypeControllerDelegate>
FakeDeviceInfoSyncService::GetControllerDelegate() {
  return fake_data_type_controller_delegate_.GetWeakPtr();
}

void FakeDeviceInfoSyncService::RefreshLocalDeviceInfo() {
  refresh_local_device_info_count_++;
}

int FakeDeviceInfoSyncService::RefreshLocalDeviceInfoCount() {
  return refresh_local_device_info_count_;
}

}  // namespace syncer
