// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_UTIL_H_
#define COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_UTIL_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "components/sync_device_info/device_info.h"

namespace sync_pb {
enum SyncEnums_DeviceType : int;
}  // namespace sync_pb

namespace syncer {

// Contains device specific names to be used by DeviceInfo. These are specific
// to the local device only. DeviceInfoSyncBridge uses either |model_name| or
// |personalizable_name| for the |client_name| depending on the current
// SyncMode. Only fully synced clients will use the personalizable name.
struct LocalDeviceNameInfo {
  LocalDeviceNameInfo();
  LocalDeviceNameInfo(const LocalDeviceNameInfo& other);
  ~LocalDeviceNameInfo();

  // Manufacturer name retrieved from SysInfo::GetHardwareInfo() - e.g. LENOVO.
  std::string manufacturer_name;
  // Model name retrieved from SysInfo::GetHardwareInfo() on non CrOS platforms.
  // On CrOS this will be set to GetChromeOSDeviceNameFromType() instead.
  std::string model_name;
  // Personalizable device name from GetPersonalizableDeviceNameBlocking(). See
  // documentation below for more information.
  std::string personalizable_name;
  // Unique hardware class string which details the
  // HW combination of a CrOS device. Empty on non-CrOS devices.
  std::string full_hardware_class;
};

sync_pb::SyncEnums_DeviceType GetLocalDeviceType();

DeviceInfo::OsType GetLocalDeviceOSType();

DeviceInfo::FormFactor GetLocalDeviceFormFactor();

// Returns the personalizable device name. This may contain
// personally-identifiable information - e.g. Alex's MacbookPro.
std::string GetPersonalizableDeviceNameBlocking();

void GetLocalDeviceNameInfo(
    base::OnceCallback<void(LocalDeviceNameInfo)> callback);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_UTIL_H_
