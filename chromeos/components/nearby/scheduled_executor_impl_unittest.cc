// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/scheduled_executor_impl.h"

#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/synchronization/lock.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "chromeos/components/nearby/library/cancelable.h"
#include "chromeos/components/nearby/library/runnable.h"
#include "chromeos/components/nearby/library/scheduled_executor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace nearby {

namespace {

constexpr base::TimeDelta kDefaultDelayTimeDelta =
    base::TimeDelta::FromMinutes(10);

class SimpleRunnable : public location::nearby::Runnable {
 public:
  explicit SimpleRunnable(base::OnceClosure closure)
      : closure_(std::move(closure)) {}
  ~SimpleRunnable() override = default;

  void run() override {
    EXPECT_FALSE(closure_.is_null());
    std::move(closure_).Run();
  }

 private:
  base::OnceClosure closure_;
};

}  // namespace

class ScheduledExecutorImplTest : public testing::Test {
 protected:
  ScheduledExecutorImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        scheduled_executor_(std::make_unique<ScheduledExecutorImpl>(
            task_environment_.GetMainThreadTaskRunner())) {}

  ~ScheduledExecutorImplTest() override = default;

  std::shared_ptr<location::nearby::Cancelable> PostRunnableWithIdAndDelay(
      const base::UnguessableToken& id,
      base::TimeDelta delay) {
    std::shared_ptr<location::nearby::Cancelable> cancelable =
        scheduled_executor_->schedule(CreateRunnable(id),
                                      delay.InMilliseconds());

    // In order to make thread-safe calls to the API of base::OneShotTimer,
    // schedule() will post a task to an internal base::SequencedTaskRunner that
    // calls Start() on a base::OneShotTimer. Executing RunUntilIdle() simply
    // ensures that the base::OneShotTimer associated with the Runnable has been
    // Start()ed, but offers no guarantee on whether the Runnable has been run()
    // or not.
    task_environment_.RunUntilIdle();

    return cancelable;
  }

  void CancelTaskAndVerifyState(
      std::shared_ptr<location::nearby::Cancelable> cancelable,
      bool should_expect_success) {
    EXPECT_EQ(should_expect_success, cancelable->cancel());

    // Ensures that the base::OneShotTimer associated with the given Cancelable
    // has been Stop()ped before this method returns.
    task_environment_.RunUntilIdle();
  }

  void VerifySetContainsId(const base::UnguessableToken& id) {
    base::AutoLock al(id_set_lock_);
    EXPECT_NE(id_set_.end(), id_set_.find(id));
  }

  size_t GetSetSize() {
    base::AutoLock al(id_set_lock_);
    return id_set_.size();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<location::nearby::ScheduledExecutor> scheduled_executor_;

 private:
  std::shared_ptr<location::nearby::Runnable> CreateRunnable(
      const base::UnguessableToken& id) {
    return std::make_shared<SimpleRunnable>(base::BindOnce(
        &ScheduledExecutorImplTest::AddIdToSet, base::Unretained(this), id));
  }

  void AddIdToSet(const base::UnguessableToken& id) {
    base::AutoLock al(id_set_lock_);
    id_set_.insert(id);
  }

  base::Lock id_set_lock_;
  std::set<base::UnguessableToken> id_set_;

  DISALLOW_COPY_AND_ASSIGN(ScheduledExecutorImplTest);
};

TEST_F(ScheduledExecutorImplTest, SingleTaskExecutes) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  PostRunnableWithIdAndDelay(id, kDefaultDelayTimeDelta);

  task_environment_.FastForwardBy(kDefaultDelayTimeDelta);
  EXPECT_EQ(1u, GetSetSize());
  VerifySetContainsId(id);
}

TEST_F(ScheduledExecutorImplTest, StaggeredTasksExecute) {
  base::UnguessableToken id0 = base::UnguessableToken::Create();
  base::UnguessableToken id1 = base::UnguessableToken::Create();
  PostRunnableWithIdAndDelay(id0, kDefaultDelayTimeDelta);
  PostRunnableWithIdAndDelay(id1, kDefaultDelayTimeDelta * 2);

  // Only the first scheduled task should run at first.
  task_environment_.FastForwardBy(kDefaultDelayTimeDelta);
  EXPECT_EQ(1u, GetSetSize());
  VerifySetContainsId(id0);

  task_environment_.FastForwardBy(kDefaultDelayTimeDelta);
  EXPECT_EQ(2u, GetSetSize());
  VerifySetContainsId(id1);
}

TEST_F(ScheduledExecutorImplTest, SingleTaskCancels) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable = PostRunnableWithIdAndDelay(id, kDefaultDelayTimeDelta);

  CancelTaskAndVerifyState(cancelable, true /* should_expect_success */);
  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  EXPECT_EQ(0u, GetSetSize());
}

TEST_F(ScheduledExecutorImplTest, FirstTaskCancelsAndSecondTaskExecutes) {
  base::UnguessableToken id0 = base::UnguessableToken::Create();
  base::UnguessableToken id1 = base::UnguessableToken::Create();
  auto cancelable0 = PostRunnableWithIdAndDelay(id0, kDefaultDelayTimeDelta);
  PostRunnableWithIdAndDelay(id1, kDefaultDelayTimeDelta * 3);

  CancelTaskAndVerifyState(cancelable0, true /* should_expect_success */);
  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  EXPECT_EQ(0u, GetSetSize());

  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  EXPECT_EQ(1u, GetSetSize());
  VerifySetContainsId(id1);
}

TEST_F(ScheduledExecutorImplTest, FailToCancelAfterRun) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable = PostRunnableWithIdAndDelay(id, kDefaultDelayTimeDelta);

  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  CancelTaskAndVerifyState(cancelable, false /* should_expect_success */);
}

TEST_F(ScheduledExecutorImplTest, FailToRunAfterCancel) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable = PostRunnableWithIdAndDelay(id, kDefaultDelayTimeDelta);

  CancelTaskAndVerifyState(cancelable, true /* should_expect_success */);
  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  EXPECT_EQ(0u, GetSetSize());
}

TEST_F(ScheduledExecutorImplTest, FailToCancelAfterCancel) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable = PostRunnableWithIdAndDelay(id, kDefaultDelayTimeDelta);

  // The first call should successfully cancel the task. Subsequent invocations
  // will return false by default, as CancelableTask uses a base::OnceClosure
  // that will be consumed after the first call to cancel().
  CancelTaskAndVerifyState(cancelable, true /* should_expect_success */);
  CancelTaskAndVerifyState(cancelable, false /* should_expect_success */);
  CancelTaskAndVerifyState(cancelable, false /* should_expect_success */);
}

TEST_F(ScheduledExecutorImplTest, FailToCancelAfterExecutorIsDestroyed) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable = PostRunnableWithIdAndDelay(id, kDefaultDelayTimeDelta);
  scheduled_executor_.reset();

  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  CancelTaskAndVerifyState(cancelable, false /* should_expect_success */);
}

TEST_F(ScheduledExecutorImplTest, FailToScheduleAfterShutdown) {
  scheduled_executor_->shutdown();
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable = PostRunnableWithIdAndDelay(id, kDefaultDelayTimeDelta);

  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  EXPECT_EQ(0u, GetSetSize());
}

TEST_F(ScheduledExecutorImplTest, FailToCancelAfterShutdown) {
  scheduled_executor_->shutdown();
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable = PostRunnableWithIdAndDelay(id, kDefaultDelayTimeDelta);

  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  CancelTaskAndVerifyState(cancelable, false /* should_expect_success */);
}

TEST_F(ScheduledExecutorImplTest, ShutdownAllowsExistingTaskToComplete) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  auto cancelable = PostRunnableWithIdAndDelay(id, kDefaultDelayTimeDelta);
  scheduled_executor_->shutdown();

  task_environment_.FastForwardBy(kDefaultDelayTimeDelta * 2);
  EXPECT_EQ(1u, GetSetSize());
  VerifySetContainsId(id);
}

}  // namespace nearby

}  // namespace chromeos
