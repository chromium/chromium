// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_CYCLE_SYNC_CYCLE_SNAPSHOT_H_
#define COMPONENTS_SYNC_ENGINE_CYCLE_SYNC_CYCLE_SNAPSHOT_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/progress_marker_map.h"
#include "components/sync/engine/cycle/model_neutral_state.h"

namespace base {
class DictionaryValue;
}

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
                    int num_encryption_conflicts,
                    int num_hierarchy_conflicts,
                    int num_server_conflicts,
                    bool notifications_enabled,
                    size_t num_entries,
                    base::Time sync_start_time,
                    base::Time poll_finish_time,
                    const std::vector<int>& num_entries_by_type,
                    const std::vector<int>& num_to_delete_entries_by_type,
                    sync_pb::SyncEnums::GetUpdatesOrigin get_updates_origin,
                    base::TimeDelta poll_interval,
                    bool has_remaining_local_changes);
  SyncCycleSnapshot(const SyncCycleSnapshot& other);
  ~SyncCycleSnapshot();

  std::unique_ptr<base::DictionaryValue> ToValue() const;

  std::string ToString() const;

  const std::string& birthday() const { return birthday_; }
  const std::string& bag_of_chips() const { return bag_of_chips_; }
  ModelNeutralState model_neutral_state() const { return model_neutral_state_; }
  const ProgressMarkerMap& download_progress_markers() const;
  bool is_silenced() const;
  int num_encryption_conflicts() const;
  int num_hierarchy_conflicts() const;
  int num_server_conflicts() const;
  bool notifications_enabled() const;
  size_t num_entries() const;
  base::Time sync_start_time() const;
  base::Time poll_finish_time() const;
  const std::vector<int>& num_entries_by_type() const;
  const std::vector<int>& num_to_delete_entries_by_type() const;
  sync_pb::SyncEnums::GetUpdatesOrigin get_updates_origin() const;
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
  int num_encryption_conflicts_;
  int num_hierarchy_conflicts_;
  int num_server_conflicts_;
  bool notifications_enabled_;
  size_t num_entries_;
  base::Time sync_start_time_;
  base::Time poll_finish_time_;

  std::vector<int> num_entries_by_type_;
  std::vector<int> num_to_delete_entries_by_type_;

  sync_pb::SyncEnums::GetUpdatesOrigin get_updates_origin_;

  base::TimeDelta poll_interval_;

  bool has_remaining_local_changes_;

  bool is_initialized_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CYCLE_SYNC_CYCLE_SNAPSHOT_H_
