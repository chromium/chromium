// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/updater/lock.h"
#include "chrome/updater/test_scope.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(LockTest, LockThenLockSameThread) {
  std::unique_ptr<ScopedLock> lock =
      ScopedLock::Create("foobar", GetTestScope(), base::Seconds(0));
  EXPECT_TRUE(lock);

  std::unique_ptr<ScopedLock> lock_again =
      ScopedLock::Create("foobar", GetTestScope(), base::Seconds(0));

#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(lock_again);
#else   // BUILDFLAG(IS_WIN)
  EXPECT_FALSE(lock_again);
#endif  // BUILDFLAG(IS_WIN)
}

TEST(LockTest, LockThenTryLockInThreadFail) {
  base::test::TaskEnvironment task_environment;

  std::unique_ptr<ScopedLock> lock =
      ScopedLock::Create("foobar", GetTestScope(), base::Seconds(0));
  EXPECT_TRUE(lock);

  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, base::BindOnce([]() {
        EXPECT_FALSE(
            ScopedLock::Create("foobar", GetTestScope(), base::Seconds(0)));
      }),
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();
}

TEST(LockTest, TryLockInThreadSuccess) {
  base::test::TaskEnvironment task_environment;

  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, base::BindOnce([]() {
        EXPECT_TRUE(
            ScopedLock::Create("foobar", GetTestScope(), base::Seconds(0)));
      }),
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();

  EXPECT_TRUE(ScopedLock::Create("foobar", GetTestScope(), base::Seconds(0)));
}

}  // namespace updater
