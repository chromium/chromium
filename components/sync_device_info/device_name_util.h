// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_NAME_UTIL_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_NAME_UTIL_H_

#include <string>

#include "components/sync_device_info/device_info.h"

namespace syncer {

struct DeviceDisplayNames {
  std::string full_name;
  std::string short_name;
};

// Returns full and short names for `device`.
DeviceDisplayNames GetDeviceDisplayNames(const DeviceInfo* device);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_NAME_UTIL_H_
