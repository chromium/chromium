// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_NAME_UTIL_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_NAME_UTIL_H_

#include <string>
#include <vector>

namespace syncer {

class DeviceInfo;

// Returns the display name for `device`, combining various sources such as
// server-provided marketing names, client-defined names, and hardware
// heuristics.
std::string GetDeviceDisplayName(const DeviceInfo* device);

// Returns a list of display names for the given `devices`.
// When `kSyncDisambiguateDeviceNamesWithChannel` is enabled, this resolves
// duplicate display names across `devices` and against `local_device` (if
// provided) by appending release channel labels.
std::vector<std::string> GetDeviceDisplayNames(
    const std::vector<const DeviceInfo*>& devices,
    const DeviceInfo* local_device = nullptr);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_NAME_UTIL_H_
