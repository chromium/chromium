// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PROCESS_PRIORITY_AGGREGATOR_DATA_H_
#define COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PROCESS_PRIORITY_AGGREGATOR_DATA_H_

#include "base/task/task_traits.h"
#include "base/values.h"
#include "components/performance_manager/graph/node_inline_data.h"

namespace performance_manager {

// This class is attached to process nodes using NodeInlineData.
class ProcessPriorityAggregatorData
    : public NodeInlineData<ProcessPriorityAggregatorData> {
 public:
  // Decrements/increments the appropriate count variable.
  void Decrement(base::TaskPriority priority);
  void Increment(base::TaskPriority priority);

  // Returns true if the various priority counts are all zero.
  bool IsEmpty() const;

  // Calculates the priority that should be upstreamed given the counts.
  base::TaskPriority GetPriority() const;

  uint32_t user_visible_count_for_testing() const {
    return user_visible_count_;
  }

  uint32_t user_blocking_count_for_testing() const {
    return user_blocking_count_;
  }

  base::Value::Dict Describe() const;

 private:
  // The number of frames at the given priority levels. The lowest priority
  // level isn't explicitly tracked as that's the default level.
#if DCHECK_IS_ON()
  // This is only tracked in DCHECK builds as a sanity check. It's not needed
  // because all processes will default to the lowest priority in the absence of
  // higher priority votes.
  uint32_t lowest_count_ = 0;
#endif
  uint32_t user_visible_count_ = 0;
  uint32_t user_blocking_count_ = 0;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PROCESS_PRIORITY_AGGREGATOR_DATA_H_
