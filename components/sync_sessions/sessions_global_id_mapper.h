// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSIONS_GLOBAL_ID_MAPPER_H_
#define COMPONENTS_SYNC_SESSIONS_SESSIONS_GLOBAL_ID_MAPPER_H_

#include <map>
#include <vector>

#include "base/time/time.h"
#include "components/sync_user_events/global_id_mapper.h"

namespace sync_sessions {

// Actual implementation of GlobalIdMapper used by sessions.
class SessionsGlobalIdMapper : public syncer::GlobalIdMapper {
 public:
  SessionsGlobalIdMapper();
  ~SessionsGlobalIdMapper();

  // GlobalIdMapper implementation.
  void AddGlobalIdChangeObserver(syncer::GlobalIdChange callback) override;
  int64_t GetLatestGlobalId(int64_t global_id) override;

  void TrackNavigationId(const base::Time& timestamp, int unique_id);

 private:
  void CleanupNavigationTracking();

  std::map<int64_t, int> global_to_unique_;
  std::map<int, int64_t> unique_to_current_global_;
  std::vector<syncer::GlobalIdChange> global_id_change_observers_;

  DISALLOW_COPY_AND_ASSIGN(SessionsGlobalIdMapper);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSIONS_GLOBAL_ID_MAPPER_H_
