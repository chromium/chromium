// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_history_utils.h"

#include <algorithm>
#include <compare>
#include <cstddef>
#include <functional>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "components/critical_actions/core/browser/critical_action_types.h"

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

  struct GroupState {
    base::Time latest_time;
    // Index of the kept action in `result[visit_id]`.
    size_t index = 0;
  };
  // Keys borrow strings from `raw_actions`, which outlives this map.
  base::flat_map<ActionGroupKey, GroupState> groups;
  for (const CriticalActionEntry& action : raw_actions) {
    if (action.action_type == ActionType::kUnknown) {
      continue;
    }

    ActionGroupKey key{.conversation_id = action.conversation_id,
                       .actor_task_id = action.actor_task_id,
                       .visit_id = action.visit_id,
                       .action_type = action.action_type};

    auto it = groups.find(key);

    // The database returns actions sorted by timestamp DESC (latest first).
    // A subsequent action falls into the tolerance window of the previous
    // action if its timestamp is greater than or equal to the
    // (last_accepted_timestamp - dedup_tolerance).
    if (it != groups.end() &&
        action.timestamp >= (it->second.latest_time - dedup_tolerance)) {
      // Duplicate: skip it, but keep its granular form fill attributes on the
      // kept action.
      if (IsGranularFormFill(action.action_type)) {
        result[action.visit_id][it->second.index].AddFormFillTypes(
            action.GetFormFillTypes());
      }
      continue;
    }

    // Remember where `action` lands, so later duplicates can merge into it.
    std::vector<CriticalActionEntry>& visit_actions = result[action.visit_id];
    groups.insert_or_assign(key, GroupState{.latest_time = action.timestamp,
                                            .index = visit_actions.size()});
    visit_actions.push_back(action);
  }

  return result;
}

}  // namespace critical_actions
