// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/active_devices_invalidation_info.h"

#include <utility>

namespace syncer {

// static
ActiveDevicesInvalidationInfo
ActiveDevicesInvalidationInfo::CreateUninitialized() {
  return ActiveDevicesInvalidationInfo(/*initialized=*/false);
}

// static
ActiveDevicesInvalidationInfo ActiveDevicesInvalidationInfo::Create(
    std::vector<std::string> fcm_registration_tokens,
    ModelTypeSet interested_data_types) {
  ActiveDevicesInvalidationInfo result(/*initialized=*/true);
  result.fcm_registration_tokens_ = std::move(fcm_registration_tokens);
  result.interested_data_types_ = interested_data_types;
  return result;
}

ActiveDevicesInvalidationInfo::ActiveDevicesInvalidationInfo(bool initialized)
    : initialized_(initialized) {}

ActiveDevicesInvalidationInfo::~ActiveDevicesInvalidationInfo() = default;

ActiveDevicesInvalidationInfo::ActiveDevicesInvalidationInfo(
    const ActiveDevicesInvalidationInfo&) = default;
ActiveDevicesInvalidationInfo& ActiveDevicesInvalidationInfo::operator=(
    const ActiveDevicesInvalidationInfo&) = default;
ActiveDevicesInvalidationInfo::ActiveDevicesInvalidationInfo(
    ActiveDevicesInvalidationInfo&&) = default;
ActiveDevicesInvalidationInfo& ActiveDevicesInvalidationInfo::operator=(
    ActiveDevicesInvalidationInfo&&) = default;

bool ActiveDevicesInvalidationInfo::IsSingleClientForTypes(
    const ModelTypeSet& types) const {
  if (!initialized_) {
    return false;
  }

  return Intersection(types, interested_data_types_).Empty();
}

}  // namespace syncer
