// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace syncer {

class DeviceInfoTracker;
class LocalDeviceInfoProvider;
class ModelTypeControllerDelegate;

// Abstract interface for a keyed service responsible for implementing sync
// datatype DEVICE_INFO and exposes information about the local device (as
// understood by sync) as well as remove syncing devices.
class DeviceInfoSyncService : public KeyedService {
 public:
  ~DeviceInfoSyncService() override;

  // Interface to get information about the local syncing device.
  virtual LocalDeviceInfoProvider* GetLocalDeviceInfoProvider() = 0;

  // Interface to get information about all syncing devices.
  virtual DeviceInfoTracker* GetDeviceInfoTracker() = 0;

  // Returns the ModelTypeControllerDelegate for DEVICE_INFO.
  virtual base::WeakPtr<ModelTypeControllerDelegate>
  GetControllerDelegate() = 0;

  // Interface to refresh local copy of device info in memory, and informs sync
  // of the change. Used when the caller knows a property of local device info
  // has changed (e.g. SharingInfo), and must be sync-ed to other devices as
  // soon as possible, without waiting for the periodic commits.
  virtual void RefreshLocalDeviceInfo() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_SERVICE_H_
