// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_model_worker.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/atomic_flag.h"
#include "base/test/test_simple_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace {

class HistoryServiceMock : public history::HistoryService {
 public:
  explicit HistoryServiceMock(
      scoped_refptr<base::SingleThreadTaskRunner> history_thread)
      : history_thread_(std::move(history_thread)) {}

  base::CancelableTaskTracker::TaskId ScheduleDBTask(
      const base::Location& from_here,
      std::unique_ptr<history::HistoryDBTask> task,
      base::CancelableTaskTracker* tracker) override {
    history::HistoryDBTask* task_raw = task.get();
    history_thread_->PostTaskAndReply(
        from_here,
        base::BindOnce(
            base::IgnoreResult(&history::HistoryDBTask::RunOnDBThread),
            base::Unretained(task_raw), nullptr, nullptr),
        base::BindOnce(&history::HistoryDBTask::DoneRunOnMainThread,
                       std::move(task)));
    return base::CancelableTaskTracker::kBadTaskId;  // Unused.
  }

 private:
  const scoped_refptr<base::SingleThreadTaskRunner> history_thread_;

  DISALLOW_COPY_AND_ASSIGN(HistoryServiceMock);
};

syncer::WorkCallback ClosureToWorkCallback(base::Closure work) {
  return base::BindOnce(
      [](base::Closure work) {
        work.Run();
        return syncer::SYNCER_OK;
      },
      std::move(work));
}

class HistoryModelWorkerTest : public testing::Test {
 public:
  HistoryModelWorkerTest()
      : sync_thread_("SyncThreadForTest"),
        history_service_(history_thread_),
        history_service_factory_(&history_service_) {
    sync_thread_.Start();
    worker_ = new HistoryModelWorker(history_service_factory_.GetWeakPtr(),
                                     ui_thread_);
  }

  ~HistoryModelWorkerTest() override {
    // Run tasks that might still have a reference to |worker_|.
    ui_thread_->RunUntilIdle();
    history_thread_->RunUntilIdle();

    // Release the last reference to |worker_|.
    EXPECT_TRUE(worker_->HasOneRef());
    worker_ = nullptr;

    // Run the DeleteSoon() task posted from ~HistoryModelWorker. This prevents
    // a leak.
    ui_thread_->RunUntilIdle();
  }

 protected:
  void DoWorkAndWaitUntilDoneOnSyncThread(base::Closure work) {
    sync_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(&HistoryModelWorker::DoWorkAndWaitUntilDone),
            worker_, ClosureToWorkCallback(work)));
    sync_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&base::AtomicFlag::Set,
                                  base::Unretained(&sync_thread_unblocked_)));
  }

  const scoped_refptr<base::TestSimpleTaskRunner> ui_thread_ =
      new base::TestSimpleTaskRunner();
  scoped_refptr<base::TestSimpleTaskRunner> history_thread_ =
      new base::TestSimpleTaskRunner();
  base::AtomicFlag sync_thread_unblocked_;
  base::Thread sync_thread_;
  HistoryServiceMock history_service_;
  scoped_refptr<HistoryModelWorker> worker_;

 private:
  base::WeakPtrFactory<HistoryServiceMock> history_service_factory_;

  DISALLOW_COPY_AND_ASSIGN(HistoryModelWorkerTest);
};

}  // namespace

TEST_F(HistoryModelWorkerTest, DoWorkAndWaitUntilDone) {
  bool did_work = false;
  DoWorkAndWaitUntilDoneOnSyncThread(base::Bind(
      [](bool* did_work) { *did_work = true; }, base::Unretained(&did_work)));

  EXPECT_FALSE(did_work);
  EXPECT_FALSE(sync_thread_unblocked_.IsSet());

  // Wait for a task to be posted to the UI thread and run it. Expect this task
  // to post another task to the history DB thread and run it.
  while (!ui_thread_->HasPendingTask())
    base::PlatformThread::YieldCurrentThread();
  ui_thread_->RunUntilIdle();
  EXPECT_TRUE(history_thread_->HasPendingTask());
  history_thread_->RunUntilIdle();

  EXPECT_TRUE(did_work);

  sync_thread_.Stop();
  EXPECT_TRUE(sync_thread_unblocked_.IsSet());
}

TEST_F(HistoryModelWorkerTest, DoWorkAndWaitUntilDoneRequestStopBeforeRunWork) {
  bool did_work = false;
  DoWorkAndWaitUntilDoneOnSyncThread(base::Bind(
      [](bool* did_work) { *did_work = true; }, base::Unretained(&did_work)));

  EXPECT_FALSE(did_work);
  EXPECT_FALSE(sync_thread_unblocked_.IsSet());

  // Wait for a task to be posted to the UI thread and run it.
  while (!ui_thread_->HasPendingTask())
    base::PlatformThread::YieldCurrentThread();
  ui_thread_->RunUntilIdle();

  // Stop the worker.
  worker_->RequestStop();

  // The WorkCallback should not run on the history DB thread.
  EXPECT_TRUE(history_thread_->HasPendingTask());
  history_thread_->RunUntilIdle();
  EXPECT_FALSE(did_work);

  sync_thread_.Stop();
  EXPECT_TRUE(sync_thread_unblocked_.IsSet());
}

TEST_F(HistoryModelWorkerTest,
       DoWorkAndWaitUntilDoneRequestStopBeforeUITaskRun) {
  bool did_work = false;
  DoWorkAndWaitUntilDoneOnSyncThread(base::Bind(
      [](bool* did_work) { *did_work = true; }, base::Unretained(&did_work)));

  EXPECT_FALSE(did_work);
  EXPECT_FALSE(sync_thread_unblocked_.IsSet());

  // Wait for a task to be posted to the UI thread.
  while (!ui_thread_->HasPendingTask())
    base::PlatformThread::YieldCurrentThread();

  // Stop the worker.
  worker_->RequestStop();

  // Stopping the worker should unblock the sync thread.
  sync_thread_.Stop();
  EXPECT_TRUE(sync_thread_unblocked_.IsSet());
}

TEST_F(HistoryModelWorkerTest, DoWorkAndWaitUntilDoneDeleteWorkBeforeRun) {
  bool did_work = false;
  DoWorkAndWaitUntilDoneOnSyncThread(base::Bind(
      [](bool* did_work) { *did_work = true; }, base::Unretained(&did_work)));

  EXPECT_FALSE(did_work);
  EXPECT_FALSE(sync_thread_unblocked_.IsSet());

  // Wait for a task to be posted to the UI thread. Delete it before it can run.
  while (!ui_thread_->HasPendingTask())
    base::PlatformThread::YieldCurrentThread();
  ui_thread_->ClearPendingTasks();

  EXPECT_FALSE(did_work);

  // Deleting the task should have unblocked the sync thread.
  sync_thread_.Stop();
  EXPECT_TRUE(sync_thread_unblocked_.IsSet());
}

TEST_F(HistoryModelWorkerTest, DoWorkAndWaitUntilDoneRequestStopDuringRunWork) {
  bool did_work = false;
  DoWorkAndWaitUntilDoneOnSyncThread(base::Bind(
      [](scoped_refptr<HistoryModelWorker> worker,
         base::AtomicFlag* sync_thread_unblocked, bool* did_work) {
        worker->RequestStop();
        base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());

        // The sync thread should not be unblocked while a WorkCallback is
        // running.
        EXPECT_FALSE(sync_thread_unblocked->IsSet());

        *did_work = true;
      },
      worker_, base::Unretained(&sync_thread_unblocked_),
      base::Unretained(&did_work)));
  EXPECT_FALSE(did_work);
  EXPECT_FALSE(sync_thread_unblocked_.IsSet());

  // Wait for a task to be posted to the UI thread and run it.
  while (!ui_thread_->HasPendingTask())
    base::PlatformThread::YieldCurrentThread();
  ui_thread_->RunUntilIdle();

  // Expect a task to be posted to the history DB thread. Run it.
  EXPECT_TRUE(history_thread_->HasPendingTask());
  history_thread_->RunUntilIdle();
  EXPECT_TRUE(did_work);

  sync_thread_.Stop();
  EXPECT_TRUE(sync_thread_unblocked_.IsSet());
}

}  // namespace browser_sync
