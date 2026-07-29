// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_NAME_UTIL_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_NAME_UTIL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"

namespace syncer {

class DeviceInfo;

// Holds candidates for the display name of a device, computed using heuristics.
// The `preferred_name_if_unique` is usually a more user-friendly representation
// of the device name.

// Note: For some OEMs, the `fallback_full_name` happens to be more
// user-friendly than the `preferred_name_if_unique`. This happens for instance
// when the manufacturer puts a marketing name instead of a raw hardware
// identifier into the model property.
// TODO(crbug.com/522788942): Remove this struct once kSyncSimplifyDeviceNaming
// is fully launched.
struct DisplayNameCandidates {
  // The preferred and usually cleaner name (e.g., "Samsung Phone",
  // "MacbookPro") shown by default if unique.
  std::string preferred_name_if_unique;

  // The more detailed name usually containing model info (e.g., "SM-S908U",
  // "MacbookPro2,3"), used as a fallback to resolve naming collisions.
  std::string fallback_full_name;
};

struct DeviceInfoWithName {
  raw_ptr<const DeviceInfo> device;
  std::string display_name;

  bool operator==(const DeviceInfoWithName& other) const = default;
};

// Returns display name candidates (primary and fallback) for `device`,
// combining various sources such as hardware heuristics, server-provided
// marketing names, and client-defined names.
DisplayNameCandidates GetDisplayNameCandidates(const DeviceInfo* device);

// Returns the display name for the given device.
// This is a simplified version that does not perform deduplication or
// filtering. It simply returns the preferred display name.
std::string GetDeviceDisplayName(const DeviceInfo* device);

// Returns a list of display names for the given devices.
// When `kSyncDisambiguateDeviceNamesWithChannel` is enabled, this resolves
// duplicate display names across `devices` and against `local_device` (if
// provided) by appending release channel labels.
std::vector<std::string> GetDeviceNames(
    const std::vector<const DeviceInfo*>& devices,
    const DeviceInfo* local_device = nullptr);

// Returns a list of display names for the given devices. This handles:
// 1. De-duplication by fallback full name: only the first occurrence in
// `devices` is
//    kept. This means the ordering of `devices` influences deduplication, as
//    entries coming earlier take precedence.
// 2. Filtering out devices with the same fallback full name as
// `local_device_name`.
// 3. Choosing between preferred and fallback full names based on whether the
//    preferred name is unique among the filtered list.
//
// TODO(crbug.com/485549442): Centralize sorting logic within this utility as
// well.
//
// `devices` should be sorted by recency (most recent first).
// Returns a list of DeviceInfoWithName. The order of `devices` is
// preserved (excluding filtered/de-duplicated entries).
// TODO(crbug.com/522788942): Remove this function once
// kSyncSimplifyDeviceNaming is fully launched.
std::vector<DeviceInfoWithName> DetermineDisplayNamesAndDeduplicate(
    const std::vector<const DeviceInfo*>& devices,
    const std::optional<std::string>& local_device_name);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_NAME_UTIL_H_
