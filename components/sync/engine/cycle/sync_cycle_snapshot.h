// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_CYCLE_SYNC_CYCLE_SNAPSHOT_H_
#define COMPONENTS_SYNC_ENGINE_CYCLE_SYNC_CYCLE_SNAPSHOT_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/progress_marker_map.h"
#include "components/sync/engine/cycle/model_neutral_state.h"

namespace sync_pb {
enum SyncEnums_GetUpdatesOrigin : int;
}  // namespace sync_pb

namespace syncer {

// An immutable snapshot of state from a SyncCycle.  Convenient to use as
// part of notifications as it is inherently thread-safe.
// TODO(zea): if copying this all over the place starts getting expensive,
// consider passing around immutable references instead of values.
// Default copy and assign welcome.
class SyncCycleSnapshot {
 public:
  SyncCycleSnapshot();
  SyncCycleSnapshot(const std::string& birthday,
                    const std::string& bag_of_chips,
                    const ModelNeutralState& model_neutral_state,
                    const ProgressMarkerMap& download_progress_markers,
                    bool is_silenced,
                    int num_server_conflicts,
                    bool notifications_enabled,
                    base::Time sync_start_time,
                    base::Time poll_finish_time,
                    sync_pb::SyncEnums_GetUpdatesOrigin get_updates_origin,
                    base::TimeDelta poll_interval,
                    bool has_remaining_local_changes);
  SyncCycleSnapshot(const SyncCycleSnapshot& other);
  ~SyncCycleSnapshot();

  base::Value::Dict ToValue() const;

  std::string ToString() const;

  const std::string& birthday() const { return birthday_; }
  const std::string& bag_of_chips() const { return bag_of_chips_; }
  ModelNeutralState model_neutral_state() const { return model_neutral_state_; }
  const ProgressMarkerMap& download_progress_markers() const;
  bool is_silenced() const;
  int num_server_conflicts() const;
  bool notifications_enabled() const;
  base::Time sync_start_time() const;
  base::Time poll_finish_time() const;
  sync_pb::SyncEnums_GetUpdatesOrigin get_updates_origin() const;
  base::TimeDelta poll_interval() const;
  // Whether usynced items existed at the time the sync cycle completed.
  bool has_remaining_local_changes() const;

  // Set iff this snapshot was not built using the default constructor.
  bool is_initialized() const;

 private:
  std::string birthday_;
  std::string bag_of_chips_;
  ModelNeutralState model_neutral_state_;
  ProgressMarkerMap download_progress_markers_;
  bool is_silenced_;
  int num_server_conflicts_;
  bool notifications_enabled_;
  base::Time sync_start_time_;
  base::Time poll_finish_time_;

  sync_pb::SyncEnums_GetUpdatesOrigin get_updates_origin_;

  base::TimeDelta poll_interval_;

  bool has_remaining_local_changes_;

  bool is_initialized_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CYCLE_SYNC_CYCLE_SNAPSHOT_H_
