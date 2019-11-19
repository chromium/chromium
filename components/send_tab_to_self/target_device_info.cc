// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/target_device_info.h"

namespace send_tab_to_self {

TargetDeviceInfo::TargetDeviceInfo(
    const std::string& device_name,
    const std::string& cache_guid,
    const sync_pb::SyncEnums::DeviceType device_type,
    base::Time last_updated_timestamp)
    : device_name(device_name),
      cache_guid(cache_guid),
      device_type(device_type),
      last_updated_timestamp(last_updated_timestamp) {}

bool TargetDeviceInfo::operator==(const TargetDeviceInfo& rhs) const {
  return this->device_name == rhs.device_name &&
         this->cache_guid == rhs.cache_guid &&
         this->device_type == rhs.device_type &&
         this->last_updated_timestamp == rhs.last_updated_timestamp;
}

}  // namespace send_tab_to_self
