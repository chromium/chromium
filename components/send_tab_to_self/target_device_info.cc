// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/target_device_info.h"

#include "base/trace_event/trace_event.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_name_util.h"

namespace send_tab_to_self {

TargetDeviceInfo::TargetDeviceInfo(
    const std::string& full_name,
    const std::string& short_name,
    const std::string& cache_guid,
    const syncer::DeviceInfo::FormFactor form_factor,
    base::Time last_updated_timestamp)
    : full_name(full_name),
      short_name(short_name),
      device_name(short_name),
      cache_guid(cache_guid),
      form_factor(form_factor),
      last_updated_timestamp(last_updated_timestamp) {}

TargetDeviceInfo::TargetDeviceInfo(const TargetDeviceInfo& other) = default;
TargetDeviceInfo::~TargetDeviceInfo() = default;

bool TargetDeviceInfo::operator==(const TargetDeviceInfo& rhs) const {
  return this->full_name == rhs.full_name &&
         this->short_name == rhs.short_name &&
         this->device_name == rhs.device_name &&
         this->cache_guid == rhs.cache_guid &&
         this->form_factor == rhs.form_factor &&
         this->last_updated_timestamp == rhs.last_updated_timestamp;
}

}  // namespace send_tab_to_self
