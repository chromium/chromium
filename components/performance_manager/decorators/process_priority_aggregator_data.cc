// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/process_priority_aggregator_data.h"

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"

namespace performance_manager {

void ProcessPriorityAggregatorData::Decrement(base::TaskPriority priority) {
  switch (priority) {
    case base::TaskPriority::LOWEST:
#if DCHECK_IS_ON()
      DCHECK_LT(0u, lowest_count_);
      --lowest_count_;
#endif
      return;

    case base::TaskPriority::USER_VISIBLE: {
      DCHECK_LT(0u, user_visible_count_);
      --user_visible_count_;
      return;
    }

    case base::TaskPriority::USER_BLOCKING: {
      DCHECK_LT(0u, user_blocking_count_);
      --user_blocking_count_;
      return;
    }
  }

  NOTREACHED_IN_MIGRATION();
}

void ProcessPriorityAggregatorData::Increment(base::TaskPriority priority) {
  switch (priority) {
    case base::TaskPriority::LOWEST:
#if DCHECK_IS_ON()
      ++lowest_count_;
#endif
      return;

    case base::TaskPriority::USER_VISIBLE: {
      ++user_visible_count_;
      return;
    }

    case base::TaskPriority::USER_BLOCKING: {
      ++user_blocking_count_;
      return;
    }
  }

  NOTREACHED_IN_MIGRATION();
}

bool ProcessPriorityAggregatorData::IsEmpty() const {
#if DCHECK_IS_ON()
  if (lowest_count_) {
    return false;
  }
#endif
  return user_blocking_count_ == 0 && user_visible_count_ == 0;
}

base::TaskPriority ProcessPriorityAggregatorData::GetPriority() const {
  if (user_blocking_count_ > 0) {
    return base::TaskPriority::USER_BLOCKING;
  }
  if (user_visible_count_ > 0) {
    return base::TaskPriority::USER_VISIBLE;
  }
  return base::TaskPriority::LOWEST;
}

base::Value::Dict ProcessPriorityAggregatorData::Describe() const {
  base::Value::Dict ret;
  ret.Set("user_visible_count", base::saturated_cast<int>(user_visible_count_));
  ret.Set("user_blocking_count",
          base::saturated_cast<int>(user_blocking_count_));
#if DCHECK_IS_ON()
  ret.Set("lowest_count", base::saturated_cast<int>(lowest_count_));
#endif  // DCHECK_IS_ON()
  return ret;
}

}  // namespace performance_manager
