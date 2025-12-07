// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_RECLAIM_TARGET_H_
#define COMPONENTS_MEMORY_PRESSURE_RECLAIM_TARGET_H_

#include <optional>

#include "base/byte_count.h"
#include "base/time/time.h"

namespace memory_pressure {

// Represents a reclaim target.
struct ReclaimTarget {
  ReclaimTarget() = default;
  ~ReclaimTarget() = default;
  explicit ReclaimTarget(base::ByteCount target) : target(target) {}
  ReclaimTarget(base::ByteCount target,
                std::optional<base::TimeTicks> origin_time,
                bool discard_protected = true)
      : target(target),
        origin_time(origin_time),
        discard_protected(discard_protected) {}

  // The amount that should be reclaimed.
  base::ByteCount target;
  // The time at which this reclaim target was calculated.
  std::optional<base::TimeTicks> origin_time;
  // Whether protected pages can be discarded.
  bool discard_protected = true;
};

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_RECLAIM_TARGET_H_
