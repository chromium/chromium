// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/model_safe_worker.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/synchronization/atomic_flag.h"
#include "base/test/test_simple_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

syncer::WorkCallback ClosureToWorkCallback(base::Closure work) {
  return base::BindOnce(
      [](base::Closure work) {
        work.Run();
        return syncer::SyncerError(syncer::SyncerError::SYNCER_OK);
      },
      std::move(work));
}

class MockModelSafeWorker : public ModelSafeWorker {
 public:
  MockModelSafeWorker() = default;

  void ScheduleWork(base::OnceClosure work) override {
    task_runner_->PostTask(FROM_HERE, std::move(work));
  }

  ModelSafeGroup GetModelSafeGroup() override { return GROUP_PASSIVE; }

  bool IsOnModelSequence() override {
    return task_runner_->BelongsToCurrentThread();
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner() const {
    return task_runner_;
  }

 private:
  friend class base::RefCountedThreadSafe<MockModelSafeWorker>;

  ~MockModelSafeWorker() override = default;

  const scoped_refptr<base::TestSimpleTaskRunner> task_runner_ =
      new base::TestSimpleTaskRunner();

  DISALLOW_COPY_AND_ASSIGN(MockModelSafeWorker);
};

class ModelSafeWorkerTest : public ::testing::Test {
 protected:
  ModelSafeWorkerTest() : sync_thread_("SyncThreadForTest") {
    sync_thread_.Start();
  }

  void DoWorkAndWaitUntilDoneOnSyncThread(base::Closure work) {
    sync_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(base::IgnoreResult(
                                      &ModelSafeWorker::DoWorkAndWaitUntilDone),
                                  worker_, ClosureToWorkCallback(work)));
    sync_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&base::AtomicFlag::Set,
                                  base::Unretained(&sync_thread_unblocked_)));
  }

  base::AtomicFlag sync_thread_unblocked_;
  base::Thread sync_thread_;
  const scoped_refptr<MockModelSafeWorker> worker_ = new MockModelSafeWorker();

 private:
  DISALLOW_COPY_AND_ASSIGN(ModelSafeWorkerTest);
};

}  // namespace

TEST_F(ModelSafeWorkerTest, ModelSafeRoutingInfoToValue) {
  ModelSafeRoutingInfo routing_info;
  routing_info[BOOKMARKS] = GROUP_PASSIVE;
  routing_info[NIGORI] = GROUP_UI;
  routing_info[PASSWORDS] = GROUP_PASSWORD;
  routing_info[APPS] = GROUP_NON_BLOCKING;
  base::DictionaryValue expected_value;
  expected_value.SetString("Apps", "Group Non Blocking");
  expected_value.SetString("Bookmarks", "Group Passive");
  expected_value.SetString("Encryption Keys", "Group UI");
  expected_value.SetString("Passwords", "Group Password");
  std::unique_ptr<base::DictionaryValue> value(
      ModelSafeRoutingInfoToValue(routing_info));
  EXPECT_TRUE(value->Equals(&expected_value));
}

TEST_F(ModelSafeWorkerTest, ModelSafeRoutingInfoToString) {
  ModelSafeRoutingInfo routing_info;
  routing_info[APPS] = GROUP_NON_BLOCKING;
  routing_info[BOOKMARKS] = GROUP_PASSIVE;
  routing_info[NIGORI] = GROUP_UI;
  routing_info[PASSWORDS] = GROUP_PASSWORD;
  EXPECT_EQ(
      "{\"Apps\":\"Group Non Blocking\",\"Bookmarks\":\"Group Passive\","
      "\"Encryption Keys\":\"Group UI\",\"Passwords\":\"Group Password\"}",
      ModelSafeRoutingInfoToString(routing_info));
}

TEST_F(ModelSafeWorkerTest, GetRoutingInfoTypes) {
  ModelSafeRoutingInfo routing_info;
  routing_info[BOOKMARKS] = GROUP_PASSIVE;
  routing_info[NIGORI] = GROUP_UI;
  routing_info[PASSWORDS] = GROUP_PASSWORD;
  const ModelTypeSet expected_types(BOOKMARKS, NIGORI, PASSWORDS);
  EXPECT_EQ(expected_types, GetRoutingInfoTypes(routing_info));
}

TEST_F(ModelSafeWorkerTest, DoWorkAndWaitUntilDone) {
  bool did_work = false;
  DoWorkAndWaitUntilDoneOnSyncThread(base::Bind(
      [](bool* did_work) { *did_work = true; }, base::Unretained(&did_work)));

  EXPECT_FALSE(did_work);
  EXPECT_FALSE(sync_thread_unblocked_.IsSet());

  // Wait for a task to be posted to |worker_|'s TaskRunner and run it.
  while (!worker_->task_runner()->HasPendingTask())
    base::PlatformThread::YieldCurrentThread();
  worker_->task_runner()->RunUntilIdle();

  EXPECT_TRUE(did_work);

  sync_thread_.Stop();
  EXPECT_TRUE(sync_thread_unblocked_.IsSet());
}

TEST_F(ModelSafeWorkerTest, DoWorkAndWaitUntilDoneRequestStopBeforeRunWork) {
  bool did_work = false;
  DoWorkAndWaitUntilDoneOnSyncThread(base::Bind(
      [](bool* did_work) { *did_work = true; }, base::Unretained(&did_work)));

  EXPECT_FALSE(did_work);
  EXPECT_FALSE(sync_thread_unblocked_.IsSet());

  // Wait for a task to be posted to |worker_|'s TaskRunner.
  while (!worker_->task_runner()->HasPendingTask())
    base::PlatformThread::YieldCurrentThread();

  // Stop the worker.
  worker_->RequestStop();

  // The WorkCallback should not run.
  worker_->task_runner()->RunUntilIdle();
  EXPECT_FALSE(did_work);

  sync_thread_.Stop();
  EXPECT_TRUE(sync_thread_unblocked_.IsSet());
}

TEST_F(ModelSafeWorkerTest, DoWorkAndWaitUntilDoneDeleteWorkBeforeRun) {
  bool did_work = false;
  DoWorkAndWaitUntilDoneOnSyncThread(base::Bind(
      [](bool* did_work) { *did_work = true; }, base::Unretained(&did_work)));

  EXPECT_FALSE(did_work);
  EXPECT_FALSE(sync_thread_unblocked_.IsSet());

  // Wait for a task to be posted to |worker_|'s TaskRunner and delete it.
  while (!worker_->task_runner()->HasPendingTask())
    base::PlatformThread::YieldCurrentThread();
  worker_->task_runner()->ClearPendingTasks();

  EXPECT_FALSE(did_work);

  // Deleting the task should have unblocked the sync thread.
  sync_thread_.Stop();
  EXPECT_TRUE(sync_thread_unblocked_.IsSet());
}

TEST_F(ModelSafeWorkerTest, DoWorkAndWaitUntilDoneRequestStopDuringRunWork) {
  bool did_work = false;
  DoWorkAndWaitUntilDoneOnSyncThread(base::Bind(
      [](scoped_refptr<ModelSafeWorker> worker,
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

  // Wait for a task to be posted to |worker_|'s TaskRunner and run it.
  while (!worker_->task_runner()->HasPendingTask())
    base::PlatformThread::YieldCurrentThread();
  worker_->task_runner()->RunUntilIdle();

  EXPECT_TRUE(did_work);
  sync_thread_.Stop();
  EXPECT_TRUE(sync_thread_unblocked_.IsSet());
}

}  // namespace syncer
