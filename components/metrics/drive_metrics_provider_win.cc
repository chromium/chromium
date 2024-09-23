// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/drive_metrics_provider.h"

#include <windows.h>

#include <winioctl.h>

#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"

namespace metrics {

// static
bool DriveMetricsProvider::HasSeekPenalty(const base::FilePath& path,
                                          bool* has_seek_penalty) {
  std::vector<base::FilePath::StringType> components = path.GetComponents();

  base::File volume(base::FilePath(L"\\\\.\\" + components[0]),
                    base::File::FLAG_OPEN);
  if (!volume.IsValid())
    return false;

  STORAGE_PROPERTY_QUERY query = {};
  query.QueryType = PropertyStandardQuery;
  query.PropertyId = StorageDeviceSeekPenaltyProperty;

  DEVICE_SEEK_PENALTY_DESCRIPTOR result;
  DWORD bytes_returned;

  BOOL success = DeviceIoControl(
      volume.GetPlatformFile(), IOCTL_STORAGE_QUERY_PROPERTY, &query,
      sizeof(query), &result, sizeof(result), &bytes_returned, nullptr);

  if (success == FALSE || bytes_returned < sizeof(result))
    return false;

  *has_seek_penalty = result.IncursSeekPenalty != FALSE;
  return true;
}

}  // namespace metrics
