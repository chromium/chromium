// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_CLIENT_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_CLIENT_H_

#include <string>
#include "base/macros.h"
#include "base/optional.h"
#include "components/sync_device_info/device_info.h"

namespace syncer {

// Interface for clients of DeviceInfoSyncService.
class DeviceInfoSyncClient {
 public:
  DeviceInfoSyncClient();
  virtual ~DeviceInfoSyncClient();

  virtual std::string GetSigninScopedDeviceId() const = 0;
  virtual bool GetSendTabToSelfReceivingEnabled() const = 0;
  virtual base::Optional<DeviceInfo::SharingInfo> GetLocalSharingInfo()
      const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceInfoSyncClient);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_CLIENT_H_
