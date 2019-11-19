// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_SYSTEM_MEMORY_STATS_RECORDER_H_
#define COMPONENTS_METRICS_SYSTEM_MEMORY_STATS_RECORDER_H_

namespace metrics {

// Record a memory size in megabytes, over a potential interval up to 32 GB.
#define UMA_HISTOGRAM_LARGE_MEMORY_MB(name, sample) \
  UMA_HISTOGRAM_CUSTOM_COUNTS(name, sample, 1, 32768, 50)

// The type of memory UMA stats to be recorded in RecordMemoryStats.
enum RecordMemoryStatsType {
  // Right after the renderer for contents was killed.
  RECORD_MEMORY_STATS_CONTENTS_OOM_KILLED,

  // Right after the renderer for extensions was killed.
  RECORD_MEMORY_STATS_EXTENSIONS_OOM_KILLED,
};

void RecordMemoryStats(RecordMemoryStatsType type);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_SYSTEM_MEMORY_STATS_RECORDER_H_
