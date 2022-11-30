// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_stats_tracker.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"

namespace {

// Global instance. Only set once, except in tests which can reset the
// instance and create a new one.
ThumbnailStatsTracker* g_instance = nullptr;

}  // namespace

// static
constexpr base::TimeDelta ThumbnailStatsTracker::kReportingInterval;

// static
ThumbnailStatsTracker& ThumbnailStatsTracker::GetInstance() {
  if (!g_instance)
    g_instance = new ThumbnailStatsTracker;
  return *g_instance;
}

// static
void ThumbnailStatsTracker::ResetInstanceForTesting() {
  delete g_instance;
  g_instance = nullptr;
}

ThumbnailStatsTracker::ThumbnailStatsTracker() {
  heartbeat_timer_.Start(
      FROM_HERE, kReportingInterval,
      base::BindRepeating(&ThumbnailStatsTracker::RecordMetrics,
                          base::Unretained(this)));
}

ThumbnailStatsTracker::~ThumbnailStatsTracker() {
  // This is only called from tests. Make sure there are no thumbnails left.
  DCHECK_EQ(thumbnails_.size(), 0u);
}

void ThumbnailStatsTracker::AddThumbnail(ThumbnailImage* thumbnail) {
  auto result = thumbnails_.insert(thumbnail);
  DCHECK(result.second) << "Thumbnail already added";
}

void ThumbnailStatsTracker::RemoveThumbnail(ThumbnailImage* thumbnail) {
  int removed = thumbnails_.erase(thumbnail);
  DCHECK_EQ(removed, 1) << "Thumbnail not added";
}

void ThumbnailStatsTracker::RecordMetrics() {
  size_t total_size_bytes = 0;

  for (ThumbnailImage* thumbnail : thumbnails_) {
    size_t thumbnail_size_bytes = thumbnail->GetCompressedDataSizeInBytes();
    total_size_bytes += thumbnail_size_bytes;

    size_t thumbnail_size_kb = thumbnail_size_bytes / 1024;
    UMA_HISTOGRAM_COUNTS_100(
        "Tab.Preview.MemoryUsage.CompressedData.PerThumbnailKiB",
        thumbnail_size_kb);
  }

  size_t total_size_kb = total_size_bytes / 1024;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Tab.Preview.MemoryUsage.CompressedData.TotalKiB",
                              total_size_kb, 32, 8192, 50);
}
