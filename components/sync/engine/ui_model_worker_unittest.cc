// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/ui_model_worker.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

// Makes a Closure into a WorkCallback.
// Does |work| and checks that we're on the |thread_verifier| thread.
SyncerError DoWork(
    const scoped_refptr<base::SingleThreadTaskRunner>& thread_verifier,
    base::Closure work) {
  DCHECK(thread_verifier->BelongsToCurrentThread());
  work.Run();
  return SyncerError(SyncerError::SYNCER_OK);
}

// Converts |work| to a WorkCallback that will verify that it's run on the
// thread it was constructed on.
WorkCallback ClosureToWorkCallback(base::Closure work) {
  return base::BindOnce(&DoWork, base::ThreadTaskRunnerHandle::Get(), work);
}

// Increments |counter|.
void IncrementCounter(int* counter) {
  ++*counter;
}

class SyncUIModelWorkerTest : public testing::Test {
 public:
  SyncUIModelWorkerTest() : sync_thread_("SyncThreadForTest") {
    sync_thread_.Start();
    worker_ = new UIModelWorker(base::ThreadTaskRunnerHandle::Get());
  }

  void PostWorkToSyncThread(base::Closure work) {
    sync_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(base::IgnoreResult(
                                      &UIModelWorker::DoWorkAndWaitUntilDone),
                                  worker_, ClosureToWorkCallback(work)));
  }

 protected:
  std::unique_ptr<base::test::SingleThreadTaskEnvironment> task_environment_ =
      std::make_unique<base::test::SingleThreadTaskEnvironment>();
  base::Thread sync_thread_;
  scoped_refptr<UIModelWorker> worker_;
};

}  // namespace

TEST_F(SyncUIModelWorkerTest, ScheduledWorkRunsOnUILoop) {
  base::RunLoop run_loop;
  PostWorkToSyncThread(run_loop.QuitClosure());
  // This won't quit until the QuitClosure is run.
  run_loop.Run();
}

TEST_F(SyncUIModelWorkerTest, MultipleDoWork) {
  constexpr int kNumWorkCallbacks = 10;
  int counter = 0;
  for (int i = 0; i < kNumWorkCallbacks; ++i) {
    PostWorkToSyncThread(
        base::Bind(&IncrementCounter, base::Unretained(&counter)));
  }

  base::RunLoop run_loop;
  PostWorkToSyncThread(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(kNumWorkCallbacks, counter);
}

TEST_F(SyncUIModelWorkerTest, JoinSyncThreadAfterUIMessageLoopDestruction) {
  PostWorkToSyncThread(base::DoNothing());

  // Wait to allow the sync thread to post the WorkCallback to the UI
  // MessageLoop. This is racy. If the WorkCallback isn't posted fast enough,
  // this test doesn't verify that UIModelWorker behaves properly when the UI
  // MessageLoop is destroyed. However, it doesn't fail (no flakes).
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  // The sync thread shouldn't wait for the WorkCallback to run on the UI thread
  // after the UI MessageLoop is gone.
  task_environment_.reset();
  sync_thread_.Stop();
}

TEST_F(SyncUIModelWorkerTest, JoinSyncThreadAfterRequestStop) {
  PostWorkToSyncThread(base::DoNothing());

  // Wait to allow the sync thread to post the WorkCallback to the UI
  // MessageLoop. This is racy. If the WorkCallback isn't posted fast enough,
  // this test doesn't verify that UIModelWorker behaves properly when
  // RequestStop() is called. However, it doesn't fail (no flakes).
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  // The sync thread shouldn't wait for the WorkCallback to run on the UI thread
  // after RequestStop() is called.
  worker_->RequestStop();
  sync_thread_.Stop();
}

}  // namespace syncer
