// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_DELETE_DIRECTIVE_HANDLER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_DELETE_DIRECTIVE_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread_checker.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/syncable_service.h"

class GURL;

namespace sync_pb {
class HistoryDeleteDirectiveSpecifics;
}

namespace history {

class HistoryDBTask;

// DeleteDirectiveHandler sends delete directives created locally to sync
// engine to propagate to other clients. It also expires local history entries
// according to given delete directives from server.
class DeleteDirectiveHandler : public syncer::SyncableService {
 public:
  // This allows injecting HistoryService::ScheduleDBTask().
  using BackendTaskScheduler =
      base::RepeatingCallback<void(const base::Location& location,
                                   std::unique_ptr<HistoryDBTask> task,
                                   base::CancelableTaskTracker* tracker)>;

  explicit DeleteDirectiveHandler(BackendTaskScheduler backend_task_scheduler);
  ~DeleteDirectiveHandler() override;

  // Notifies that HistoryBackend has been fully loaded and hence is ready to
  // handle sync events.
  void OnBackendLoaded();

  // Create delete directives for the deletion of visits identified by
  // |global_ids| (which may be empty), in the time range specified by
  // |begin_time| and |end_time|.
  bool CreateDeleteDirectives(const std::set<int64_t>& global_ids,
                              base::Time begin_time,
                              base::Time end_time);

  bool CreateUrlDeleteDirective(const GURL& url);

  // Sends the given |delete_directive| to SyncChangeProcessor (if it exists).
  // Returns any error resulting from sending the delete directive to sync.
  // NOTE: the given |delete_directive| is not processed to remove local
  //       history entries that match. Caller still needs to call other
  //       interfaces, e.g. HistoryService::ExpireHistoryBetween(), to delete
  //       local history entries.
  syncer::SyncError ProcessLocalDeleteDirective(
      const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive);

  // syncer::SyncableService implementation.
  void WaitUntilReadyToSync(base::OnceClosure done) override;
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> error_handler) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

 private:
  class DeleteDirectiveTask;
  friend class DeleteDirectiveTask;

  // Action to take on processed delete directives.
  enum PostProcessingAction { KEEP_AFTER_PROCESSING, DROP_AFTER_PROCESSING };

  // Callback when history backend finishes deleting visits according to
  // |delete_directives|.
  void FinishProcessing(PostProcessingAction post_processing_action,
                        const syncer::SyncDataList& delete_directives);

  const BackendTaskScheduler backend_task_scheduler_;
  bool backend_loaded_ = false;
  base::OnceClosure wait_until_ready_to_sync_cb_;
  base::CancelableTaskTracker internal_tracker_;
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<DeleteDirectiveHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeleteDirectiveHandler);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_DELETE_DIRECTIVE_HANDLER_H_
