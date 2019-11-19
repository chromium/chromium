// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_SERVICE_IMPL_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "components/sync/model/model_type_store.h"
#include "components/sync_device_info/device_info_sync_service.h"

namespace syncer {

class DeviceInfoPrefs;
class DeviceInfoSyncClient;
class DeviceInfoSyncBridge;
class MutableLocalDeviceInfoProvider;

class DeviceInfoSyncServiceImpl : public DeviceInfoSyncService {
 public:
  // |local_device_info_provider| must not be null.
  // |device_info_prefs| must not be null.
  // |device_info_sync_client| must not be null and must outlive this object.
  DeviceInfoSyncServiceImpl(
      OnceModelTypeStoreFactory model_type_store_factory,
      std::unique_ptr<MutableLocalDeviceInfoProvider>
          local_device_info_provider,
      std::unique_ptr<DeviceInfoPrefs> device_info_prefs,
      std::unique_ptr<DeviceInfoSyncClient> device_info_sync_client);
  ~DeviceInfoSyncServiceImpl() override;

  // DeviceInfoSyncService implementation.
  LocalDeviceInfoProvider* GetLocalDeviceInfoProvider() override;
  DeviceInfoTracker* GetDeviceInfoTracker() override;
  base::WeakPtr<ModelTypeControllerDelegate> GetControllerDelegate() override;
  void RefreshLocalDeviceInfo() override;

 private:
  std::unique_ptr<DeviceInfoSyncClient> device_info_sync_client_;
  std::unique_ptr<DeviceInfoSyncBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(DeviceInfoSyncServiceImpl);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_SERVICE_IMPL_H_
