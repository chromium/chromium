// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_STORAGE_DURABILITY_H_
#define CHROME_BROWSER_WIN_STORAGE_DURABILITY_H_

#include <stdint.h>

#include "base/feature_list.h"
#include "base/files/file_path.h"

namespace storage_durability {

// The goal here is to determine whether the drive holding the profile can be
// trusted to have actually written what the browser asked it to write. There
// are unexplained cases where folks are losing `Local State`, including their
// OSCrypt secret. This could be explained by folks having a dangerous setting
// enabled when an event happens (crash, etc).
//
// Windows exposes a per-device setting via Device Manager. Under "Disk drives",
// specific disks are shown. When inspecting Properties for a given disk, under
// the "Policies" tab, "Turn off Windows write-cache buffer flushing on the
// device" tells Windows to stop sending flush commands. Microsoft's own UI
// warns that this can cause data loss on power failure.
//
// This setting can be read using the SDK via
// `STORAGE_WRITE_CACHE_PROPERTY::UserDefinedPowerProtection`, documented in
// winioctl.h as "User selected power protection option through registry". It is
// not exposed through WMI. The way to read it is `DeviceIoControl()` with
// `IOCTL_STORAGE_QUERY_PROPERTY` and `StorageDeviceWriteCacheProperty`.
//
// NOTE: the setting is not always dangerous or wrong. On hardware with a
// battery-backed cache it is the correct configuration, and the device reports
// that separately as `NVCacheEnabled`.
//
// How the fields of `STORAGE_WRITE_CACHE_PROPERTY` line up with the Policies
// tab of a disk's Properties dialog in Device Manager. Only two of them appear
// in that UI; the rest describe the hardware.
//
//   "Enable write caching on the device"
//     `WriteCacheEnabled`, a `WRITE_CACHE_ENABLE`:
//       0 WriteCacheEnableUnknown  the driver would not say
//       1 WriteCacheDisabled       unticked
//       2 WriteCacheEnabled        ticked
//
//   "Turn off Windows write-cache buffer flushing on the device"
//     `UserDefinedPowerProtection`, a `BOOLEAN`, 1 when ticked, 0 when not.
//     Matches the value of `CacheIsPowerProtected` REG_DWORD under the disk's
//     `Device Parameters\Disk` key under
//     `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum`. Disabled in the UI
//     unless `WriteCacheEnabled` is ticked. NOTE: this behavior does seem to
//     differ from the description "indicates whether a user can configure":
//     https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ns-winioctl-storage_write_cache_property
//
// The remaining fields have no UI:
//   `WriteCacheType`, a `WRITE_CACHE_TYPE`:
//       0 unknown, 1 none, 2 write back, 3 write through
//   `WriteCacheChangeable`, a `WRITE_CACHE_CHANGE`:
//       0 unknown, 1 not changeable, 2 changeable
//   `WriteThroughSupported`, a `WRITE_THROUGH`:
//       0 unknown, 1 not supported, 2 supported
//   `FlushCacheSupported`, a `BOOLEAN`, the drive honors a flush request.
//   `NVCacheEnabled`, a `BOOLEAN`, the cache is battery backed, which is what
//       makes the second checkbox above reasonable rather than dangerous.
//
// Each of these enums has 0 as "unknown". Zero is the absence of an answer.
// A healthy consumer NVMe drive with both checkboxes at their defaults answers
// `WriteCacheType` 2,
// `WriteCacheEnabled` 2,
// `WriteCacheChangeable` 0,
// `WriteThroughSupported` 2,
// `FlushCacheSupported` 1

namespace features {

// Gates the whole check. `RecordStorageDurabilityMetrics` tests this itself, so
// no caller can ask the drive anything while it is off.
BASE_DECLARE_FEATURE(kStorageDurabilityCheck);

}  // namespace features

// Asks the drive holding `user_data_dir` what it does with writes and reports
// the answer to UMA. Blocking, so call it from a `MayBlock()` task. Does
// nothing when `kStorageDurabilityCheck` is off.
//
// This only measures how common the configuration is. Nothing is shown to the
// user yet; see crbug.com/578581696.
void RecordStorageDurabilityMetrics(const base::FilePath& user_data_dir);

namespace internal {

// What the drive answered. Only created via `QueryStorageDurability()`.
struct StorageDurability {
  bool queried = false;
  // The drive is caching writes in volatile memory.
  bool write_cache_enabled = false;
  // The driver answered but would not say either way, so
  // `write_cache_enabled` being false does not mean caching is off.
  bool write_cache_state_unknown = false;
  // "Turn off Windows write-cache buffer flushing on the device" is on.
  bool user_defined_power_protection = false;
  // The cache is battery backed, which is what makes the setting above
  // legitimate rather than dangerous.
  bool nv_cache_enabled = false;
  // The drive reports that it cannot flush its cache on request.
  bool flush_cache_supported = true;
};

// Whether drive state describes a drive that can lose an already-renamed file
// on power loss. True only when writes are cached, Windows has been told not to
// flush them, and nothing is backing the cache up.
bool IsWriteCacheFlushingUnsafe(const StorageDurability& durability);

// Asks the drive holding `path`. Blocking, and returns `queried == false` when
// the drive cannot be asked, which includes network paths and any volume that
// is not reached through a drive letter.
//
// Exposed for testing.
StorageDurability QueryStorageDurability(const base::FilePath& path);

}  // namespace internal

}  // namespace storage_durability

#endif  // CHROME_BROWSER_WIN_STORAGE_DURABILITY_H_
