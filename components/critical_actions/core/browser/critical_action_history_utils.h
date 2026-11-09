// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_HISTORY_UTILS_H_
#define COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_HISTORY_UTILS_H_

#include <cstdint>
#include <vector>

#include "base/time/time.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace critical_actions {

// Critical actions keyed by the History visit ID they were performed on. Within
// each visit, actions keep the order returned by the database (latest first).
using CriticalActionsByVisitId =
    absl::flat_hash_map<int64_t, std::vector<CriticalActionEntry>>;

// Default tolerance used to merge duplicate logs of the same event. 5 seconds
// is chosen as a safe heuristic upper bound to accommodate potential latency
// delays between the Actor and Chrome side logs of the same event, while being
// small enough to avoid merging separate events.
inline constexpr base::TimeDelta kDefaultCriticalActionDedupTolerance =
    base::Seconds(5);

// Deduplicates `raw_actions` and groups them by `visit_id`. `raw_actions` must
// be sorted by timestamp, latest first (as returned by
// CriticalActionProvider::GetCriticalActions()).
//
// Two actions are considered duplicates if they share the same conversation
// ID, actor task ID, visit ID and action type, and the older one falls within
// `dedup_tolerance` of the most recently kept one. Actions of type
// ActionType::kUnknown are dropped.
CriticalActionsByVisitId GroupCriticalActionsByVisitId(
    const std::vector<CriticalActionEntry>& raw_actions,
    base::TimeDelta dedup_tolerance = kDefaultCriticalActionDedupTolerance);

}  // namespace critical_actions

#endif  // COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_HISTORY_UTILS_H_
