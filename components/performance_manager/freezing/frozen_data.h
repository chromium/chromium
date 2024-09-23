// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FROZEN_DATA_H_
#define COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FROZEN_DATA_H_

#include "base/values.h"
#include "components/performance_manager/graph/node_inline_data.h"
#include "components/performance_manager/public/mojom/lifecycle.mojom.h"

namespace performance_manager {

class FrozenData : public NodeInlineData<FrozenData> {
 public:
  // Returns the current "is_frozen" state. A collection of frames is considered
  // frozen if its non-empty, and all of the frames are frozen.
  bool IsFrozen() const;

  // Returns the state as an equivalent LifecycleState.
  performance_manager::mojom::LifecycleState AsLifecycleState() const;

  // Applies a change to frame counts. Returns true if that causes the frozen
  // state to change for this object.
  bool ChangeFrameCounts(int32_t current_frame_delta,
                         int32_t frozen_frame_delta);

  base::Value::Dict Describe();

  uint32_t current_frame_count() const { return current_frame_count_; }

  uint32_t frozen_frame_count() const { return frozen_frame_count_; }

 private:
  // The number of current frames associated with a given page/process.
  uint32_t current_frame_count_ = 0;

  // The number of frozen current frames associated with a given page/process.
  // This is always <= |current_frame_count|.
  uint32_t frozen_frame_count_ = 0;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FROZEN_DATA_H_
