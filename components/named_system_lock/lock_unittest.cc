// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_system_lock/lock.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_LINUX)
#include <fcntl.h>
#include <sys/mman.h>

#include "base/files/file.h"
#include "base/strings/strcat.h"
#elif BUILDFLAG(IS_MAC)
#include "base/strings/strcat.h"
#elif BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/strings/utf_string_conversions.h"
#include "base/win/atl.h"
#endif

namespace named_system_lock {

class NamedSystemLockTest : public ::testing::Test {
 public:
  ~NamedSystemLockTest() override = default;

 protected:
  // Use a different lock name for each test to avoid failures due to concurrent
  // runs or leaks.
  const std::string lock_name_ = base::UnguessableToken::Create().ToString();

  std::unique_ptr<ScopedLock> CreateLock() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
    return ScopedLock::Create(lock_name_, base::Seconds(0));
#else
    CSecurityAttributes sa;
    return ScopedLock::Create(base::ASCIIToWide(lock_name_), &sa,
                              base::Seconds(0));
#endif
  }
};

TEST_F(NamedSystemLockTest, LockThenLockSameThread) {
  std::unique_ptr<ScopedLock> lock = CreateLock();
  EXPECT_TRUE(lock);

  std::unique_ptr<ScopedLock> lock_again = CreateLock();

#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(lock_again);
#else   // BUILDFLAG(IS_WIN)
  EXPECT_FALSE(lock_again);
#endif  // BUILDFLAG(IS_WIN)
}

TEST_F(NamedSystemLockTest, LockThenTryLockInThreadFail) {
  base::test::TaskEnvironment task_environment;

  std::unique_ptr<ScopedLock> lock = CreateLock();
  EXPECT_TRUE(lock);

  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindLambdaForTesting([&] { EXPECT_FALSE(CreateLock()); }),
      base::BindLambdaForTesting([&run_loop] { run_loop.Quit(); }));
  run_loop.Run();
}

TEST_F(NamedSystemLockTest, TryLockInThreadSuccess) {
  base::test::TaskEnvironment task_environment;

  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindLambdaForTesting([&] { EXPECT_TRUE(CreateLock()); }),
      base::BindLambdaForTesting([&run_loop] { run_loop.Quit(); }));
  run_loop.Run();

  EXPECT_TRUE(CreateLock());
}

#if BUILDFLAG(IS_LINUX)
TEST_F(NamedSystemLockTest, SharedMemoryWrongPermissions) {
  // Create a shared memory region with overpermissive perms.
  int shm_fd = shm_open(lock_name_.c_str(), O_RDWR | O_CREAT | O_EXCL,
                        S_IRWXU | S_IRWXG | S_IRWXO);
  ASSERT_GE(shm_fd, 0);

  EXPECT_FALSE(CreateLock());

  close(shm_fd);
  shm_unlink(lock_name_.c_str());
}
#endif  // BUILDFLAG(IS_LINUX)

}  // namespace named_system_lock
