// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/storage_durability.h"

#include <windows.h>

#include <winioctl.h>

#include <cstddef>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_handle.h"

namespace storage_durability {

namespace features {

BASE_FEATURE(kStorageDurabilityCheck, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features

namespace {

// Opens the volume `path` sits on, for querying only. Zero access rights are
// requested deliberately: `IOCTL_STORAGE_QUERY_PROPERTY` does not need read
// access, and asking for it would require privileges the browser does not have.
// NOTE: it's possible this path may involve a junction point if the user
// configured one. But that shouldn't be the case for most folks.
base::win::ScopedHandle OpenVolumeForQuery(const base::FilePath& path) {
  std::wstring_view value = path.value();
  // Only a drive-letter path identifies a volume we can name this way. A UNC
  // path is a network share, which is recorded as its own risk elsewhere and
  // has no local write cache to ask about.
  if (value.size() < 2 || value[1] != L':') {
    return base::win::ScopedHandle();
  }

  const std::wstring volume_path =
      base::StrCat({LR"(\\.\)", value.substr(0, 2)});
  return base::win::ScopedHandle(
      ::CreateFile(volume_path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                   nullptr, OPEN_EXISTING, 0, nullptr));
}

}  // namespace

void RecordStorageDurabilityMetrics(const base::FilePath& user_data_dir) {
  if (!base::FeatureList::IsEnabled(features::kStorageDurabilityCheck)) {
    return;
  }

  const internal::StorageDurability durability =
      internal::QueryStorageDurability(user_data_dir);

  base::UmaHistogramBoolean("Windows.StorageDurability.Queried",
                            durability.queried);
  if (!durability.queried) {
    return;
  }

  base::UmaHistogramBoolean("Windows.StorageDurability.WriteCacheEnabled",
                            durability.write_cache_enabled);
  base::UmaHistogramBoolean("Windows.StorageDurability.WriteCacheStateUnknown",
                            durability.write_cache_state_unknown);
  base::UmaHistogramBoolean(
      "Windows.StorageDurability.UserDefinedPowerProtection",
      durability.user_defined_power_protection);
  base::UmaHistogramBoolean("Windows.StorageDurability.NVCacheEnabled",
                            durability.nv_cache_enabled);
  base::UmaHistogramBoolean("Windows.StorageDurability.FlushCacheSupported",
                            durability.flush_cache_supported);
  base::UmaHistogramBoolean("Windows.StorageDurability.FlushingUnsafe",
                            IsWriteCacheFlushingUnsafe(durability));
}

namespace internal {

bool IsWriteCacheFlushingUnsafe(const StorageDurability& durability) {
  if (!durability.queried) {
    return false;
  }
  // A battery-backed cache is what makes the Device Manager setting a
  // reasonable choice rather than a dangerous one, so it clears the risk.
  if (durability.nv_cache_enabled) {
    return false;
  }
  return durability.write_cache_enabled &&
         durability.user_defined_power_protection;
}

StorageDurability QueryStorageDurability(const base::FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  StorageDurability durability;

  base::win::ScopedHandle volume = OpenVolumeForQuery(path);
  if (!volume.is_valid()) {
    return durability;
  }

  STORAGE_PROPERTY_QUERY query = {.PropertyId = StorageDeviceWriteCacheProperty,
                                  .QueryType = PropertyStandardQuery};

  STORAGE_WRITE_CACHE_PROPERTY result = {};
  // Everything through `NVCacheEnabled`; the struct's trailing
  // padding is not required of the driver.
  constexpr DWORD kMinimumResultSize =
      offsetof(STORAGE_WRITE_CACHE_PROPERTY, NVCacheEnabled) +
      sizeof(result.NVCacheEnabled);
  DWORD bytes_returned = 0;
  if (!::DeviceIoControl(volume.get(), IOCTL_STORAGE_QUERY_PROPERTY, &query,
                         sizeof(query), &result, sizeof(result),
                         &bytes_returned, /*lpOverlapped=*/nullptr) ||
      bytes_returned < kMinimumResultSize || result.Size < kMinimumResultSize) {
    // Not every driver answers this, so an unhelpful drive is expected rather
    // than exceptional.
    return durability;
  }

  durability.queried = true;
  durability.write_cache_enabled =
      result.WriteCacheEnabled == WriteCacheEnabled;
  durability.write_cache_state_unknown =
      result.WriteCacheEnabled == WriteCacheEnableUnknown;
  durability.user_defined_power_protection =
      result.UserDefinedPowerProtection != FALSE;
  durability.nv_cache_enabled = result.NVCacheEnabled != FALSE;
  durability.flush_cache_supported = result.FlushCacheSupported != FALSE;

  return durability;
}

}  // namespace internal

}  // namespace storage_durability
