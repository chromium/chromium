// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/sessions_global_id_mapper.h"

#include <map>
#include <utility>

namespace sync_sessions {
namespace {

// Clean up navigation tracking when we have over this many global_ids.
const size_t kNavigationTrackingCleanupThreshold = 100;

// When we clean up navigation tracking, delete this many global_ids.
const int kNavigationTrackingCleanupAmount = 10;

}  // namespace

SessionsGlobalIdMapper::SessionsGlobalIdMapper() = default;

SessionsGlobalIdMapper::~SessionsGlobalIdMapper() = default;

void SessionsGlobalIdMapper::AddGlobalIdChangeObserver(
    syncer::GlobalIdChange callback) {
  global_id_change_observers_.push_back(std::move(callback));
}

int64_t SessionsGlobalIdMapper::GetLatestGlobalId(int64_t global_id) {
  auto g2u_iter = global_to_unique_.find(global_id);
  if (g2u_iter != global_to_unique_.end()) {
    auto u2g_iter = unique_to_current_global_.find(g2u_iter->second);
    if (u2g_iter != unique_to_current_global_.end()) {
      return u2g_iter->second;
    }
  }
  return global_id;
}

void SessionsGlobalIdMapper::TrackNavigationId(const base::Time& timestamp,
                                               int unique_id) {
  // The expectation is that global_id will update for a given unique_id, which
  // should accurately and uniquely represent a single navigation. It is
  // theoretically possible for two unique_ids to map to the same global_id, but
  // hopefully rare enough that it doesn't cause much harm. Lets record metrics
  // verify this theory.
  int64_t global_id = timestamp.ToInternalValue();
  // It is possible that the global_id has not been set yet for this navigation.
  // In this case there's nothing here for us to track yet.
  if (global_id == 0) {
    return;
  }

  DCHECK_NE(0, unique_id);

  global_to_unique_.emplace(global_id, unique_id);

  auto u2g_iter = unique_to_current_global_.find(unique_id);
  if (u2g_iter == unique_to_current_global_.end()) {
    unique_to_current_global_.insert(u2g_iter,
                                     std::make_pair(unique_id, global_id));
  } else if (u2g_iter->second != global_id) {
    // Remember the old_global_id before we insert and invalidate out iter.
    int64_t old_global_id = u2g_iter->second;

    // TODO(skym): Use insert_or_assign with hint once on C++17.
    unique_to_current_global_[unique_id] = global_id;

    // This should be done after updating unique_to_current_global_ in case one
    // of our observers calls into GetLatestGlobalId().
    for (auto& observer : global_id_change_observers_) {
      observer.Run(old_global_id, global_id);
    }
  }

  CleanupNavigationTracking();
}

void SessionsGlobalIdMapper::CleanupNavigationTracking() {
  DCHECK(kNavigationTrackingCleanupThreshold >
         kNavigationTrackingCleanupAmount);

  // |global_to_unique_| is implicitly ordered by least recently created, which
  //  means we can drop from the beginning.
  if (global_to_unique_.size() > kNavigationTrackingCleanupThreshold) {
    auto iter = global_to_unique_.begin();
    std::advance(iter, kNavigationTrackingCleanupAmount);
    global_to_unique_.erase(global_to_unique_.begin(), iter);

    // While |unique_id|s do get bigger for the most part, this isn't a great
    // thing to make assumptions about, and an old tab may get refreshed often
    // and still be very important. So instead just delete anything that's
    // orphaned from |global_to_unique_|.
    std::erase_if(unique_to_current_global_,
                  [this](const std::pair<int, int64_t>& kv) {
                    return !global_to_unique_.contains(kv.second);
                  });
  }
}

}  // namespace sync_sessions
