// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/count_down_latch.h"

#include <memory>
#include <optional>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby {
namespace chrome {

class CountDownLatchTest : public testing::Test {
 protected:
  void InitializeCountDownLatch(int32_t count) {
    count_down_latch_ = std::make_unique<CountDownLatch>(count);
  }

  void PostAwaitTask(base::RunLoop& run_loop,
                     const base::UnguessableToken& attempt_id,
                     std::optional<base::TimeDelta> timeout) {
    base::RunLoop wait_run_loop;
    auto callback = base::BindLambdaForTesting([&, timeout]() {
      base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;

      wait_run_loop.Quit();

      std::optional<ExceptionOr<bool>> result;
      if (timeout) {
        result = count_down_latch_->Await(
            absl::Microseconds(timeout->InMicroseconds()));
      } else {
        result = ExceptionOr<bool>(count_down_latch_->Await());
      }

      {
        base::AutoLock al(map_lock_);
        id_to_result_map_.emplace(attempt_id, *result);
      }

      run_loop.Quit();
    });
    task_runner_->PostTask(FROM_HERE, std::move(callback));

    // Wait until callback has started.
    wait_run_loop.Run();
  }

  size_t MapSize() {
    base::AutoLock al(map_lock_);
    return id_to_result_map_.size();
  }

  void VerifyExceptionResultForAttemptId(
      const base::UnguessableToken& id,
      const Exception::Value& expected_exception) {
    base::AutoLock al(map_lock_);
    ASSERT_TRUE(base::Contains(id_to_result_map_, id));
    ASSERT_TRUE(id_to_result_map_[id]);
    EXPECT_EQ(expected_exception, id_to_result_map_[id]->exception());
  }

  void VerifyBoolResultForAttemptId(const base::UnguessableToken& id,
                                    bool expected_result) {
    base::AutoLock al(map_lock_);
    ASSERT_TRUE(base::Contains(id_to_result_map_, id));
    ASSERT_TRUE(id_to_result_map_[id]);
    EXPECT_EQ(expected_result, id_to_result_map_[id]->ok());
    EXPECT_EQ(expected_result, id_to_result_map_[id]->result());
  }

  base::test::TaskEnvironment task_environment_;
  base::Lock map_lock_;
  base::flat_map<base::UnguessableToken, std::optional<ExceptionOr<bool>>>
      id_to_result_map_;
  std::unique_ptr<CountDownLatch> count_down_latch_;

 private:
  scoped_refptr<base::TaskRunner> task_runner_ =
      base::ThreadPool::CreateTaskRunner({base::MayBlock()});
};

TEST_F(CountDownLatchTest, InitializeCount0_AwaitTimed_DoesNotBlock) {
  InitializeCountDownLatch(0);

  base::RunLoop run_loop;
  base::UnguessableToken attempt_id = base::UnguessableToken::Create();
  PostAwaitTask(run_loop, attempt_id, base::Milliseconds(1000));

  run_loop.Run();
  EXPECT_EQ(1u, MapSize());
  VerifyBoolResultForAttemptId(attempt_id, true);
}

TEST_F(CountDownLatchTest, InitializeCount0_AwaitInf_DoesNotBlock) {
  InitializeCountDownLatch(0);

  base::RunLoop run_loop;
  base::UnguessableToken attempt_id = base::UnguessableToken::Create();
  PostAwaitTask(run_loop, attempt_id, std::nullopt /* timeout */);

  run_loop.Run();
  EXPECT_EQ(1u, MapSize());
  VerifyExceptionResultForAttemptId(attempt_id, Exception::kSuccess);
}

TEST_F(CountDownLatchTest, InitializeCount2_BlocksUnlessCountIsZero) {
  InitializeCountDownLatch(2);

  base::RunLoop run_loop;
  base::UnguessableToken attempt_id = base::UnguessableToken::Create();
  PostAwaitTask(run_loop, attempt_id, std::nullopt /* timeout */);
  ASSERT_EQ(0u, MapSize());

  count_down_latch_->CountDown();
  EXPECT_EQ(0u, MapSize());

  count_down_latch_->CountDown();
  run_loop.Run();
  EXPECT_EQ(1u, MapSize());
  VerifyExceptionResultForAttemptId(attempt_id, Exception::kSuccess);

  // Further CountDown is ignored.
  count_down_latch_->CountDown();
  EXPECT_EQ(1u, MapSize());
  VerifyExceptionResultForAttemptId(attempt_id, Exception::kSuccess);
}

// TODO(crbug.com/1185706): Hangs on ChromeOS MSAN.
#if BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_InitializeCount2_UnblocksAllBlockedThreadsWhenCountIsZero \
  DISABLED_InitializeCount2_UnblocksAllBlockedThreadsWhenCountIsZero
#else
#define MAYBE_InitializeCount2_UnblocksAllBlockedThreadsWhenCountIsZero \
  InitializeCount2_UnblocksAllBlockedThreadsWhenCountIsZero
#endif
TEST_F(CountDownLatchTest,
       MAYBE_InitializeCount2_UnblocksAllBlockedThreadsWhenCountIsZero) {
  InitializeCountDownLatch(2);

  base::RunLoop run_loop_1;
  base::UnguessableToken attempt_id_1 = base::UnguessableToken::Create();
  PostAwaitTask(run_loop_1, attempt_id_1, std::nullopt /* timeout */);
  base::RunLoop run_loop_2;
  base::UnguessableToken attempt_id_2 = base::UnguessableToken::Create();
  PostAwaitTask(run_loop_2, attempt_id_2, std::nullopt /* timeout */);
  base::RunLoop run_loop_3;
  base::UnguessableToken attempt_id_3 = base::UnguessableToken::Create();
  PostAwaitTask(run_loop_3, attempt_id_3, std::nullopt /* timeout */);
  ASSERT_EQ(0u, MapSize());

  count_down_latch_->CountDown();
  ASSERT_EQ(0u, MapSize());

  count_down_latch_->CountDown();

  run_loop_1.Run();
  run_loop_2.Run();
  run_loop_3.Run();
  EXPECT_EQ(3u, MapSize());
  VerifyExceptionResultForAttemptId(attempt_id_1, Exception::kSuccess);
  VerifyExceptionResultForAttemptId(attempt_id_2, Exception::kSuccess);
  VerifyExceptionResultForAttemptId(attempt_id_3, Exception::kSuccess);
}

TEST_F(CountDownLatchTest, InitializeCount2_TimedAwaitTimesOut) {
  InitializeCountDownLatch(2);

  base::RunLoop run_loop;
  base::UnguessableToken attempt_id = base::UnguessableToken::Create();
  PostAwaitTask(run_loop, attempt_id, base::Milliseconds(1000));

  run_loop.Run();
  EXPECT_EQ(1u, MapSize());
  VerifyBoolResultForAttemptId(attempt_id, false);
}

TEST_F(CountDownLatchTest, InitializeCount2_LongerTimedAwaitDoesNotTimeOut) {
  InitializeCountDownLatch(2);

  base::RunLoop run_loop_1;
  base::UnguessableToken attempt_id_1 = base::UnguessableToken::Create();
  PostAwaitTask(run_loop_1, attempt_id_1, base::Milliseconds(100));
  base::RunLoop run_loop_2;
  base::UnguessableToken attempt_id_2 = base::UnguessableToken::Create();
  PostAwaitTask(run_loop_2, attempt_id_2, base::Milliseconds(1000));

  run_loop_1.Run();
  ASSERT_EQ(1u, MapSize());
  VerifyBoolResultForAttemptId(attempt_id_1, false);

  count_down_latch_->CountDown();
  ASSERT_EQ(1u, MapSize());

  count_down_latch_->CountDown();

  run_loop_2.Run();
  EXPECT_EQ(2u, MapSize());
  VerifyBoolResultForAttemptId(attempt_id_2, true);
}

}  // namespace chrome
}  // namespace nearby
