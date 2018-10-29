// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_DELETE_DIRECTIVE_HANDLER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_DELETE_DIRECTIVE_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread_checker.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_data.h"

namespace sync_pb {
class HistoryDeleteDirectiveSpecifics;
}

namespace history {

class HistoryService;

// DeleteDirectiveHandler sends delete directives created locally to sync
// engine to propagate to other clients. It also expires local history entries
// according to given delete directives from server.
class DeleteDirectiveHandler {
 public:
  DeleteDirectiveHandler();
  ~DeleteDirectiveHandler();

  // Start/stop processing delete directives when sync is enabled/disabled.
  void Start(HistoryService* history_service,
             const syncer::SyncDataList& initial_sync_data,
             std::unique_ptr<syncer::SyncChangeProcessor> sync_processor);
  void Stop();

  // Create delete directives for the deletion of visits identified by
  // |global_ids| (which may be empty), in the time range specified by
  // |begin_time| and |end_time|.
  bool CreateDeleteDirectives(const std::set<int64_t>& global_ids,
                              base::Time begin_time,
                              base::Time end_time);

  // Sends the given |delete_directive| to SyncChangeProcessor (if it exists).
  // Returns any error resulting from sending the delete directive to sync.
  // NOTE: the given |delete_directive| is not processed to remove local
  //       history entries that match. Caller still needs to call other
  //       interfaces, e.g. HistoryService::ExpireHistoryBetween(), to delete
  //       local history entries.
  syncer::SyncError ProcessLocalDeleteDirective(
      const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive);

  // Expires local history entries according to delete directives from server.
  syncer::SyncError ProcessSyncChanges(
      HistoryService* history_service,
      const syncer::SyncChangeList& change_list);

 private:
  class DeleteDirectiveTask;
  friend class DeleteDirectiveTask;

  // Action to take on processed delete directives.
  enum PostProcessingAction { KEEP_AFTER_PROCESSING, DROP_AFTER_PROCESSING };

  // Callback when history backend finishes deleting visits according to
  // |delete_directives|.
  void FinishProcessing(PostProcessingAction post_processing_action,
                        const syncer::SyncDataList& delete_directives);

  base::CancelableTaskTracker internal_tracker_;
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<DeleteDirectiveHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeleteDirectiveHandler);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_DELETE_DIRECTIVE_HANDLER_H_
