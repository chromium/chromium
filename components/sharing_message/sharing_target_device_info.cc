// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_target_device_info.h"

SharingTargetDeviceInfo::SharingTargetDeviceInfo(
    const std::string& guid,
    const std::string& client_name,
    SharingDevicePlatform platform,
    base::TimeDelta pulse_interval,
    syncer::DeviceInfo::FormFactor form_factor,
    base::Time last_updated_timestamp)
    : guid_(guid),
      client_name_(client_name),
      platform_(platform),
      pulse_interval_(pulse_interval),
      form_factor_(form_factor),
      last_updated_timestamp_(last_updated_timestamp) {}

SharingTargetDeviceInfo::SharingTargetDeviceInfo(SharingTargetDeviceInfo&&) =
    default;

SharingTargetDeviceInfo::~SharingTargetDeviceInfo() = default;

SharingTargetDeviceInfo& SharingTargetDeviceInfo::operator=(
    SharingTargetDeviceInfo&&) = default;
