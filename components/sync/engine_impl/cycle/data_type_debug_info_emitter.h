// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_CYCLE_DATA_TYPE_DEBUG_INFO_EMITTER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_CYCLE_DATA_TYPE_DEBUG_INFO_EMITTER_H_

#include <memory>

#include "base/macros.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/cycle/commit_counters.h"
#include "components/sync/engine/cycle/update_counters.h"

namespace base {
class HistogramBase;
}

namespace syncer {

// Supports various kinds of debugging requests for a certain directory type.
//
// TODO(crbug.com/1102849): Rename Emit*() methods to mention UMA, and update
// the documentation to not mention any observers.
// The Emit*() functions send updates to registered TypeDebugInfoObservers.
// The DataTypeDebugInfoEmitter does not directly own that list; it is
// managed by the ModelTypeRegistry.
//
// For Update and Commit counters, the job of keeping the counters up to date
// is delegated to the UpdateHandler and CommitContributors. For the Stats
// counters, the emitter will let sub class to fetch all the required
// information on demand.
class DataTypeDebugInfoEmitter {
 public:
  explicit DataTypeDebugInfoEmitter(ModelType type);

  virtual ~DataTypeDebugInfoEmitter();

  // Returns a reference to the current commit counters.
  const CommitCounters& GetCommitCounters() const;

  // Allows others to mutate the commit counters.
  CommitCounters* GetMutableCommitCounters();

  // Triggers a commit counters update to registered observers.
  void EmitCommitCountersUpdate();

  // Returns a reference to the current update counters.
  const UpdateCounters& GetUpdateCounters() const;

  // Allows others to mutate the update counters.
  UpdateCounters* GetMutableUpdateCounters();

  // Triggers an update counters update to registered observers.
  void EmitUpdateCountersUpdate();

 private:
  // The actual up-to-date counters.
  CommitCounters commit_counters_;
  UpdateCounters update_counters_;

  // The last state of the counters emitted to UMA. In the next round of
  // emitting to UMA, we only need to upload the diff between the actual
  // counters and the counts here.
  CommitCounters emitted_commit_counters_;
  UpdateCounters emitted_update_counters_;

  // The histogram to record to; cached for efficiency because many histogram
  // entries are recorded in this object during run-time.
  base::HistogramBase* const histogram_;

  DISALLOW_COPY_AND_ASSIGN(DataTypeDebugInfoEmitter);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_CYCLE_DATA_TYPE_DEBUG_INFO_EMITTER_H_
