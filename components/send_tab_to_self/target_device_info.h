// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_TARGET_DEVICE_INFO_H_
#define COMPONENTS_SEND_TAB_TO_SELF_TARGET_DEVICE_INFO_H_

#include <string>

#include "base/time/time.h"
#include "components/sync_device_info/device_info.h"

namespace syncer {
class DeviceInfo;
}  // namespace syncer

namespace send_tab_to_self {

struct SharingDeviceNames {
  std::string full_name;
  std::string short_name;
};

// Device information for generating send tab to self UI.
struct TargetDeviceInfo {
 public:
  TargetDeviceInfo(const std::string& full_name,
                   const std::string& short_name,
                   const std::string& cache_guid,
                   const syncer::DeviceInfo::FormFactor form_factor,
                   base::Time last_updated_timestamp);
  TargetDeviceInfo(const TargetDeviceInfo& other);
  ~TargetDeviceInfo();

  bool operator==(const TargetDeviceInfo& rhs) const;

  // Device full name.
  std::string full_name;
  // Device short name.
  std::string short_name;
  // Device name
  std::string device_name;
  // Device guid.
  std::string cache_guid;
  // Device Form Factor.
  syncer::DeviceInfo::FormFactor form_factor;
  // Last updated timestamp.
  base::Time last_updated_timestamp;
};

// Returns full and short names for |device|.
SharingDeviceNames GetSharingDeviceNames(const syncer::DeviceInfo* device);

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_TARGET_DEVICE_INFO_H_
