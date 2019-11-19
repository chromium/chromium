// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_FAKE_DEVICE_INFO_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_DEVICE_INFO_FAKE_DEVICE_INFO_SYNC_SERVICE_H_

#include "components/sync/model/fake_model_type_controller_delegate.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_device_info/fake_local_device_info_provider.h"

namespace syncer {

class FakeDeviceInfoSyncService : public DeviceInfoSyncService {
 public:
  FakeDeviceInfoSyncService();
  ~FakeDeviceInfoSyncService() override;

  // DeviceInfoSyncService implementation.
  FakeLocalDeviceInfoProvider* GetLocalDeviceInfoProvider() override;
  FakeDeviceInfoTracker* GetDeviceInfoTracker() override;
  base::WeakPtr<ModelTypeControllerDelegate> GetControllerDelegate() override;
  void RefreshLocalDeviceInfo() override;

  // Returns number of times RefreshLocalDeviceInfo() has neen called.
  int RefreshLocalDeviceInfoCount();

 private:
  FakeDeviceInfoTracker fake_device_info_tracker_;
  FakeLocalDeviceInfoProvider fake_local_device_info_provider_;
  FakeModelTypeControllerDelegate fake_model_type_controller_delegate_;

  int refresh_local_device_info_count_ = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_FAKE_DEVICE_INFO_SYNC_SERVICE_H_
