// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sequenced_model_worker.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/timer/timer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class SequencedModelWorkerTest : public testing::Test {
 public:
  SequencedModelWorkerTest() : did_do_work_(false) {}

  bool did_do_work() { return did_do_work_; }
  SequencedModelWorker* worker() { return worker_.get(); }
  base::OneShotTimer* timer() { return &timer_; }
  base::WeakPtrFactory<SequencedModelWorkerTest>* factory() {
    return &weak_factory_;
  }

  // Schedule DoWork to be executed on the DB sequence and have the test fail if
  // DoWork hasn't executed within action_timeout().
  void ScheduleWork() {
    // We wait until the callback is done. So it is safe to use unretained.
    timer()->Start(FROM_HERE, TestTimeouts::action_timeout(), this,
                   &SequencedModelWorkerTest::Timeout);
    worker()->DoWorkAndWaitUntilDone(base::BindOnce(
        &SequencedModelWorkerTest::DoWork, base::Unretained(this)));
  }

  // This is the work that will be scheduled to be done on the DB sequence.
  SyncerError DoWork() {
    EXPECT_TRUE(task_runner_->RunsTasksInCurrentSequence());
    run_loop_.Quit();
    did_do_work_ = true;
    return SyncerError(SyncerError::SYNCER_OK);
  }

  // This will be called by the OneShotTimer and make the test fail unless
  // DoWork is called first.
  void Timeout() {
    ADD_FAILURE()
        << "Timed out waiting for work to be done on the DB sequence.";
    run_loop_.Quit();
  }

 protected:
  void SetUp() override {
    task_runner_ =
        base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                         base::TaskPriority::BEST_EFFORT});
    worker_ = new SequencedModelWorker(task_runner_, GROUP_PASSWORD);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  bool did_do_work_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<SequencedModelWorker> worker_;
  base::OneShotTimer timer_;

 protected:
  base::RunLoop run_loop_;

 private:
  base::WeakPtrFactory<SequencedModelWorkerTest> weak_factory_{this};
};

TEST_F(SequencedModelWorkerTest, DoesWorkOnDatabaseSequence) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SequencedModelWorkerTest::ScheduleWork,
                                factory()->GetWeakPtr()));
  run_loop_.Run();
  EXPECT_TRUE(did_do_work());
}

}  // namespace

}  // namespace syncer
