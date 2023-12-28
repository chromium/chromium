// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_RECLAIM_TARGET_H_
#define COMPONENTS_MEMORY_PRESSURE_RECLAIM_TARGET_H_

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace memory_pressure {

// Represents a reclaim target.
struct ReclaimTarget {
  ReclaimTarget() = default;
  ~ReclaimTarget() = default;
  explicit ReclaimTarget(uint64_t target_kb) : target_kb(target_kb) {}
  ReclaimTarget(uint64_t target_kb, absl::optional<base::TimeTicks> origin_time)
      : target_kb(target_kb), origin_time(origin_time) {}

  // The number of KiB that should be reclaimed.
  uint64_t target_kb = 0;
  // The time at which this reclaim target was calculated.
  absl::optional<base::TimeTicks> origin_time;
};

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_RECLAIM_TARGET_H_
