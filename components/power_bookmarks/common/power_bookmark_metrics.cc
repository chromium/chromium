// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/common/power_bookmark_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace {
const int kBytesPerKB = 1024;
}  // namespace

namespace power_bookmarks::metrics {

void RecordPowerCreated(sync_pb::PowerBookmarkSpecifics::PowerType power_type,
                        bool success) {
  base::UmaHistogramBoolean("PowerBookmarks.PowerCreated.Success", success);
  if (success) {
    // Here and below - using macros are necessary to record enums coming
    // generated protobuf code. This is because ARRAYSIZE is used as the max
    // (MAX isn't the correct value) which is incompatible with the histogram
    // functions because it's an int and doesn't pass the type checking.
    UMA_HISTOGRAM_ENUMERATION(
        "PowerBookmarks.PowerCreated.PowerType", power_type,
        sync_pb::PowerBookmarkSpecifics::PowerType_ARRAYSIZE);
  }
}

void RecordPowerUpdated(sync_pb::PowerBookmarkSpecifics::PowerType power_type,
                        bool success) {
  base::UmaHistogramBoolean("PowerBookmarks.PowerUpdated.Success", success);
  if (success) {
    UMA_HISTOGRAM_ENUMERATION(
        "PowerBookmarks.PowerUpdated.PowerType", power_type,
        sync_pb::PowerBookmarkSpecifics::PowerType_ARRAYSIZE);
  }
}

void RecordPowerDeleted(bool success) {
  base::UmaHistogramBoolean("PowerBookmarks.PowerDeleted.Success", success);
}

void RecordPowersDeletedForURL(
    sync_pb::PowerBookmarkSpecifics::PowerType power_type,
    bool success) {
  base::UmaHistogramBoolean("PowerBookmarks.PowersDeletedForURL.Success",
                            success);
  if (success) {
    UMA_HISTOGRAM_ENUMERATION(
        "PowerBookmarks.PowersDeletedForURL.PowerType", power_type,
        sync_pb::PowerBookmarkSpecifics::PowerType_ARRAYSIZE);
  }
}

void RecordDatabaseError(int error) {
  base::UmaHistogramSparse("PowerBookmarks.Storage.DatabaseError", error);
}

void RecordDatabaseSizeAtStartup(int64_t size_in_bytes) {
  int size_in_kb = base::saturated_cast<int>(size_in_bytes / kBytesPerKB);
  base::UmaHistogramCounts1M("PowerBookmarks.Storage.DatabaseDirSizeAtStartup",
                             size_in_kb);
}

}  // namespace power_bookmarks::metrics