// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_model_worker.h"

#include <memory>
#include <utility>

#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"

namespace browser_sync {

class WorkerTask : public history::HistoryDBTask {
 public:
  explicit WorkerTask(base::OnceClosure work) : work_(std::move(work)) {}

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    std::move(work_).Run();
    return true;
  }

  // Since the DoWorkAndWaitUntilDone() is synchronous, we don't need to run
  // any code asynchronously on the main thread after completion.
  void DoneRunOnMainThread() override {}

 private:
  // A OnceClosure is deleted right after it runs. This is important to unblock
  // DoWorkAndWaitUntilDone() right after the task runs.
  base::OnceClosure work_;

  DISALLOW_COPY_AND_ASSIGN(WorkerTask);
};

namespace {

// Post the work task on |history_service|'s DB thread from the UI
// thread.
void PostWorkerTask(
    const base::WeakPtr<history::HistoryService>& history_service,
    base::OnceClosure work,
    base::CancelableTaskTracker* cancelable_tracker) {
  if (history_service) {
    history_service->ScheduleDBTask(
        FROM_HERE, std::make_unique<WorkerTask>(std::move(work)),
        cancelable_tracker);
  }
}

}  // namespace

HistoryModelWorker::HistoryModelWorker(
    const base::WeakPtr<history::HistoryService>& history_service,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread)
    : history_service_(history_service),
      ui_thread_(std::move(ui_thread)),
      // base::OnTaskRunnerDeleter ensures that |cancelable_tracker_| is
      // destroyed on the UI thread, even if |this| is deleted on a different
      // thread. It is a requirement of base::CancelableTaskTracker to be
      // constructed and deleted on the same sequence.
      cancelable_tracker_(new base::CancelableTaskTracker,
                          base::OnTaskRunnerDeleter(ui_thread_)) {
  CHECK(history_service);
  DCHECK(ui_thread_->BelongsToCurrentThread());
}

syncer::ModelSafeGroup HistoryModelWorker::GetModelSafeGroup() {
  return syncer::GROUP_HISTORY;
}

bool HistoryModelWorker::IsOnModelSequence() {
  // Ideally HistoryService would expose a way to check whether this is the
  // history DB thread. Since it doesn't, just return true to bypass a CHECK in
  // the sync code.
  return true;
}

HistoryModelWorker::~HistoryModelWorker() = default;

void HistoryModelWorker::ScheduleWork(base::OnceClosure work) {
  ui_thread_->PostTask(
      FROM_HERE, base::BindOnce(&PostWorkerTask, history_service_,
                                std::move(work), cancelable_tracker_.get()));
}

}  // namespace browser_sync
