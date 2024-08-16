// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_COMMIT_H_
#define COMPONENTS_SYNC_ENGINE_COMMIT_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>

#include "components/sync/base/data_type.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/engine/commit_contribution.h"
#include "components/sync/engine/cycle/nudge_tracker.h"
#include "components/sync/engine/syncer_error.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

class ActiveDevicesInvalidationInfo;
class CommitProcessor;
class StatusController;
class SyncCycle;

// This class wraps the actions related to building and executing a single
// commit operation.
//
// This class' most important responsibility is to manage the ContributionsMap.
// This class serves as a container for those objects.  Although it would have
// been acceptable to let this class be a dumb container object, it turns out
// that there was no other convenient place to put the Init() and
// PostAndProcessCommitResponse() functions.  So they ended up here.
class Commit {
 public:
  using ContributionMap =
      std::map<DataType, std::unique_ptr<CommitContribution>>;

  Commit(ContributionMap contributions,
         const sync_pb::ClientToServerMessage& message,
         ExtensionsActivity::Records extensions_activity_buffer);

  Commit(const Commit&) = delete;
  Commit& operator=(const Commit&) = delete;

  ~Commit();

  // |extensions_activity| may be null.
  static std::unique_ptr<Commit> Init(
      DataTypeSet enabled_types,
      size_t max_entries,
      const std::string& account_name,
      const std::string& cache_guid,
      bool cookie_jar_mismatch,
      const ActiveDevicesInvalidationInfo& active_devices_invalidation_info,
      CommitProcessor* commit_processor,
      ExtensionsActivity* extensions_activity);

  // |extensions_activity| may be null.
  SyncerError PostAndProcessResponse(NudgeTracker* nudge_tracker,
                                     SyncCycle* cycle,
                                     StatusController* status,
                                     ExtensionsActivity* extensions_activity);

  DataTypeSet GetContributingDataTypes() const;

 private:
  // Report commit failure to each contribution.
  void ReportFullCommitFailure(SyncerError syncer_error);

  ContributionMap contributions_;

  sync_pb::ClientToServerMessage message_;
  ExtensionsActivity::Records extensions_activity_buffer_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_COMMIT_H_
