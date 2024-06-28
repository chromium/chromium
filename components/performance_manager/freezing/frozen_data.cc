// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/freezing/frozen_data.h"

#include "base/check_op.h"

namespace performance_manager {

bool FrozenData::IsFrozen() const {
  return current_frame_count_ > 0 &&
         frozen_frame_count_ == current_frame_count_;
}

// Returns the state as an equivalent LifecycleState.
performance_manager::mojom::LifecycleState FrozenData::AsLifecycleState()
    const {
  if (IsFrozen()) {
    return performance_manager::mojom::LifecycleState::kFrozen;
  }
  return performance_manager::mojom::LifecycleState::kRunning;
}

bool FrozenData::ChangeFrameCounts(int32_t current_frame_delta,
                                   int32_t frozen_frame_delta) {
  // Each delta should be -1, 0 or 1.
  DCHECK(current_frame_delta != 0 || frozen_frame_delta != 0);
  DCHECK_GE(1, abs(current_frame_delta));
  DCHECK_GE(1, abs(frozen_frame_delta));
  // We should never have (-1, 1) or (1, -1).
  DCHECK_NE(-current_frame_delta, frozen_frame_delta);

  // If the deltas are negative, the counts need to be positive.
  DCHECK(current_frame_delta >= 0 || current_frame_count_ > 0);
  DCHECK(frozen_frame_delta >= 0 || frozen_frame_count_ > 0);

  bool was_frozen = IsFrozen();
  current_frame_count_ += current_frame_delta;
  frozen_frame_count_ += frozen_frame_delta;

  return IsFrozen() != was_frozen;
}

base::Value::Dict FrozenData::Describe() {
  base::Value::Dict ret;
  ret.Set("current_frame_count", static_cast<int>(current_frame_count_));
  ret.Set("frozen_frame_count", static_cast<int>(frozen_frame_count_));
  return ret;
}

}  // namespace performance_manager
