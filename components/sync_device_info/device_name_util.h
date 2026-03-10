// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_NAME_UTIL_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_NAME_UTIL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "components/sync_device_info/device_info.h"

namespace syncer {

struct DeviceDisplayNames {
  std::string full_name;
  std::string short_name;
};

struct DeviceInfoWithName {
  raw_ptr<const DeviceInfo> device;
  std::string display_name;
};

// Returns full and short names for `device`.
DeviceDisplayNames GetDeviceDisplayNames(const DeviceInfo* device);

// Returns a list of display names for the given devices. This handles:
// 1. De-duplication by full name: only the first occurrence in `devices` is
//    kept. This means the ordering of `devices` influences deduplication, as
//    entries coming earlier take precedence.
// 2. Filtering out devices with the same full name as `local_device_name`.
// 3. Choosing between short and full names based on whether the short name is
//    unique among the filtered list.
//
// TODO(crbug.com/485549442): Centralize sorting logic within this utility as
// well.
//
// `devices` should be sorted by recency (most recent first).
// Returns a list of DeviceInfoWithName. The order of `devices` is
// preserved (excluding filtered/de-duplicated entries).
std::vector<DeviceInfoWithName> DetermineDisplayNamesAndDeduplicate(
    const std::vector<const DeviceInfo*>& devices,
    const std::optional<std::string>& local_device_name);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_NAME_UTIL_H_
