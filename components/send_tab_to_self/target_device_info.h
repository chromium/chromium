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

// Device information for generating send tab to self UI.
struct TargetDeviceInfo {
 public:
  TargetDeviceInfo(std::string device_name,
                   std::string cache_guid,
                   const syncer::DeviceInfo::FormFactor form_factor,
                   base::Time last_updated_timestamp,
                   bool has_high_precision_timestamp = false);
  TargetDeviceInfo(const TargetDeviceInfo& other);
  ~TargetDeviceInfo();

  bool operator==(const TargetDeviceInfo& rhs) const;

  // Returns a localized string representing the time since the device was last
  // updated.
  // The string is formatted as follows:
  // - "< 1 minute": "Active now"
  // - ">= 1 minute": "Active X minutes/hours/days ago"
  std::u16string GetLastActiveTimeForDisplay() const;

  // Device display name.
  std::string device_name;
  // Device guid.
  std::string cache_guid;
  // Device Form Factor.
  syncer::DeviceInfo::FormFactor form_factor;
  // Last updated timestamp.
  base::Time last_updated_timestamp;
  // Whether the device timestamp is highly precise (e.g. from sessions sync)
  // rather than just day-granularity.
  bool has_high_precision_timestamp;
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_TARGET_DEVICE_INFO_H_
