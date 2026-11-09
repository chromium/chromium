// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_history_utils.h"

#include <algorithm>
#include <compare>
#include <functional>
#include <string_view>

#include "base/check.h"
#include "base/containers/flat_map.h"

namespace critical_actions {

namespace {

struct ActionGroupKey {
  std::string_view conversation_id;
  std::string_view actor_task_id;
  int64_t visit_id;
  ActionType action_type;
  auto operator<=>(const ActionGroupKey&) const = default;
};

}  // namespace

CriticalActionsByVisitId GroupCriticalActionsByVisitId(
    const std::vector<CriticalActionEntry>& raw_actions,
    base::TimeDelta dedup_tolerance) {
  CHECK(std::ranges::is_sorted(raw_actions, std::ranges::greater{},
                               &CriticalActionEntry::timestamp));

  CriticalActionsByVisitId result;
  if (raw_actions.empty()) {
    return result;
  }

  // Keys borrow strings from `raw_actions`, which outlives this map.
  base::flat_map<ActionGroupKey, base::Time> group_latest_times;
  for (const CriticalActionEntry& action : raw_actions) {
    ActionGroupKey key{.conversation_id = action.conversation_id,
                       .actor_task_id = action.actor_task_id,
                       .visit_id = action.visit_id,
                       .action_type = action.action_type};

    auto it = group_latest_times.find(key);

    // The database returns actions sorted by timestamp DESC (latest first).
    // A subsequent action falls into the tolerance window of the previous
    // action if its timestamp is greater than or equal to the
    // (last_accepted_timestamp - dedup_tolerance).
    if (it != group_latest_times.end() &&
        action.timestamp >= (it->second - dedup_tolerance)) {
      continue;  // It's a duplicate, skip it.
    }
    group_latest_times.insert_or_assign(key, action.timestamp);

    if (action.action_type == ActionType::kUnknown) {
      continue;
    }
    result[action.visit_id].push_back(action);
  }

  return result;
}

}  // namespace critical_actions
