// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/resource_id.h"

namespace viz {

ReservedResourceIdTracker::ReservedResourceIdTracker() = default;
ReservedResourceIdTracker::~ReservedResourceIdTracker() = default;

ResourceId ReservedResourceIdTracker::AllocId(int initial_ref_count) {
  // Check we can allocate an ID.
  CHECK_LT(id_ref_counts_.size(), kNumReservedResourceIds);

  // Find the next available ID. This is guaranteed to terminate because we
  // checked there is at least one available ID.
  while (id_ref_counts_.contains(ResourceId(next_id_))) {
    if (next_id_ == std::numeric_limits<uint32_t>::max()) {
      next_id_ = kVizReservedRangeStartId.GetUnsafeValue();
    } else {
      ++next_id_;
    }
  }
  DCHECK_GE(next_id_, kVizReservedRangeStartId.GetUnsafeValue());
  auto id = ResourceId(next_id_);
  id_ref_counts_[id] = initial_ref_count;
  return id;
}

void ReservedResourceIdTracker::RefId(ResourceId id, int count) {
  auto iter = id_ref_counts_.find(id);
  CHECK(iter != id_ref_counts_.end());
  iter->second += count;
}

bool ReservedResourceIdTracker::UnrefId(ResourceId id, int count) {
  auto iter = id_ref_counts_.find(id);
  CHECK(iter != id_ref_counts_.end());
  CHECK_GE(iter->second, count);
  iter->second -= count;
  if (iter->second == 0) {
    id_ref_counts_.erase(iter);
    return true;
  }
  return false;
}

}  // namespace viz
