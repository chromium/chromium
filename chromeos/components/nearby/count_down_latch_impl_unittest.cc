// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/count_down_latch_impl.h"

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace nearby {

class CountDownLatchImplTest : public testing::Test {
 protected:
  CountDownLatchImplTest() = default;

  void InitializeCountDownLatch(int32_t count) {
    count_down_latch_ = std::make_unique<CountDownLatchImpl>(count);
  }

  void WaitForAllPostedTasksToFinish() {
    base::ThreadPoolInstance::Get()->FlushForTesting();
  }

  // Returns a unique ID for the posted AwaitTask(). This ID can be used in
  // |id_to_result_map_| to check if the associated AwaitTask() has completed.
  base::UnguessableToken PostAwaitTask(
      const base::Optional<base::TimeDelta>& timeout_millis) {
    base::UnguessableToken unique_task_id = base::UnguessableToken::Create();
    base::PostTask(
        FROM_HERE, {base::ThreadPool(), base::MayBlock()},
        base::BindOnce(&CountDownLatchImplTest::AwaitTask,
                       base::Unretained(this), timeout_millis, unique_task_id));
    return unique_task_id;
  }

  location::nearby::CountDownLatch* count_down_latch() {
    return count_down_latch_.get();
  }

  size_t GetMapSize() {
    base::AutoLock al(map_lock_);
    return id_to_result_map_.size();
  }

  void VerifyExceptionResultForAttemptId(
      const base::UnguessableToken& id,
      const location::nearby::Exception::Value& expected_exception) {
    base::AutoLock al(map_lock_);
    EXPECT_TRUE(base::Contains(id_to_result_map_, id));
    EXPECT_TRUE(id_to_result_map_[id]);
    EXPECT_EQ(expected_exception, id_to_result_map_[id]->exception());
  }

  void VerifyBoolResultForAttemptId(const base::UnguessableToken& id,
                                    bool expected_result) {
    base::AutoLock al(map_lock_);
    EXPECT_TRUE(base::Contains(id_to_result_map_, id));
    EXPECT_TRUE(id_to_result_map_[id]);
    EXPECT_TRUE(id_to_result_map_[id]->ok());
    EXPECT_EQ(expected_result, id_to_result_map_[id]->result());
  }

  // It's necessary to sleep after calling PostAwaitTask() if we're expecting
  // AwaitTask() to hang due to |count_| not being at 0. Otherwise it's
  // ambiguous whether AwaitTask() correctly blocked or not. For instance, if we
  // were to check whether |id_to_result_map_| is empty following a call to
  // PostAwaitTask() while |count_| is 2, we wouldn't know whether 1)
  // |id_to_result_map_| is empty because AwaitTask() is correctly blocking, or
  // 2) |id_to_result_map_| is empty because we checked it before AwaitTask()
  // was able to erroneously populate it.
  void TinyTimeout() {
    // As of writing, tiny_timeout() is 100ms.
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }

  base::test::TaskEnvironment task_environment_;
  base::Lock map_lock_;
  base::flat_map<base::UnguessableToken,
                 base::Optional<location::nearby::ExceptionOr<bool>>>
      id_to_result_map_;

 private:
  void AwaitTask(const base::Optional<base::TimeDelta>& timeout_millis,
                 const base::UnguessableToken& unique_task_id) {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;

    base::Optional<location::nearby::ExceptionOr<bool>> result;
    if (timeout_millis) {
      result = count_down_latch_->await(timeout_millis->InMilliseconds());
    } else {
      result = location::nearby::ExceptionOr<bool>(count_down_latch_->await());
    }

    base::AutoLock al(map_lock_);
    id_to_result_map_.emplace(unique_task_id, *result);
  }

  std::unique_ptr<location::nearby::CountDownLatch> count_down_latch_;

  DISALLOW_COPY_AND_ASSIGN(CountDownLatchImplTest);
};

TEST_F(CountDownLatchImplTest, InitializeCount0_AwaitTimed_DoesNotBlock) {
  InitializeCountDownLatch(0);

  base::UnguessableToken id =
      PostAwaitTask(base::TimeDelta::FromMilliseconds(1000));
  WaitForAllPostedTasksToFinish();
  EXPECT_EQ(1u, GetMapSize());
  VerifyBoolResultForAttemptId(id, true);
}

TEST_F(CountDownLatchImplTest, InitializeCount0_AwaitInf_DoesNotBlock) {
  InitializeCountDownLatch(0);

  base::UnguessableToken id = PostAwaitTask(base::nullopt /* timeout_millis */);
  WaitForAllPostedTasksToFinish();
  EXPECT_EQ(1u, GetMapSize());
  VerifyExceptionResultForAttemptId(id, location::nearby::Exception::NONE);
}

TEST_F(CountDownLatchImplTest, InitializeCount2_BlocksUnlessCountIsZero) {
  InitializeCountDownLatch(2);

  base::UnguessableToken id = PostAwaitTask(base::nullopt /* timeout_millis */);
  TinyTimeout();
  EXPECT_EQ(0u, GetMapSize());

  count_down_latch()->countDown();
  TinyTimeout();
  EXPECT_EQ(0u, GetMapSize());

  count_down_latch()->countDown();
  WaitForAllPostedTasksToFinish();
  EXPECT_EQ(1u, GetMapSize());
  VerifyExceptionResultForAttemptId(id, location::nearby::Exception::NONE);
}

TEST_F(CountDownLatchImplTest,
       InitializeCount2_UnblocksAllBlockedThreadsWhenCountIsZero) {
  InitializeCountDownLatch(2);

  base::UnguessableToken id0 =
      PostAwaitTask(base::nullopt /* timeout_millis */);
  base::UnguessableToken id1 =
      PostAwaitTask(base::nullopt /* timeout_millis */);
  base::UnguessableToken id2 =
      PostAwaitTask(base::nullopt /* timeout_millis */);

  TinyTimeout();
  EXPECT_EQ(0u, GetMapSize());

  count_down_latch()->countDown();
  TinyTimeout();
  EXPECT_EQ(0u, GetMapSize());

  count_down_latch()->countDown();
  WaitForAllPostedTasksToFinish();
  EXPECT_EQ(3u, GetMapSize());

  VerifyExceptionResultForAttemptId(id0, location::nearby::Exception::NONE);
  VerifyExceptionResultForAttemptId(id1, location::nearby::Exception::NONE);
  VerifyExceptionResultForAttemptId(id2, location::nearby::Exception::NONE);
}

TEST_F(CountDownLatchImplTest, InitializeCount2_TimedAwaitTimesOut) {
  InitializeCountDownLatch(2);

  base::UnguessableToken id =
      PostAwaitTask(base::TimeDelta::FromMilliseconds(1000));
  WaitForAllPostedTasksToFinish();
  EXPECT_EQ(1u, GetMapSize());
  VerifyBoolResultForAttemptId(id, false);
}

TEST_F(CountDownLatchImplTest,
       InitializeCount2_LongerTimedAwaitDoesNotTimeOut) {
  InitializeCountDownLatch(2);

  base::UnguessableToken id_shorter = PostAwaitTask(
      TestTimeouts::tiny_timeout() - base::TimeDelta::FromMilliseconds(10));
  base::UnguessableToken id_longer = PostAwaitTask(
      2 * TestTimeouts::tiny_timeout() - base::TimeDelta::FromMilliseconds(10));

  TinyTimeout();
  EXPECT_EQ(1u, GetMapSize());
  VerifyBoolResultForAttemptId(id_shorter, false);

  count_down_latch()->countDown();
  count_down_latch()->countDown();
  TinyTimeout();
  EXPECT_EQ(2u, GetMapSize());
  VerifyBoolResultForAttemptId(id_longer, true);
}

}  // namespace nearby

}  // namespace chromeos
