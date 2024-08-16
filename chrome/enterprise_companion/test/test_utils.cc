// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/test/test_utils.h"

#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"

namespace enterprise_companion {

int WaitForProcess(base::Process& process) {
  int exit_code = 0;
  bool process_exited = false;
  base::RunLoop wait_for_process_exit_loop;
  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTaskAndReply(
          FROM_HERE, base::BindLambdaForTesting([&] {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow_blocking;
            process_exited = process.WaitForExitWithTimeout(
                TestTimeouts::action_timeout(), &exit_code);
          }),
          wait_for_process_exit_loop.QuitClosure());
  wait_for_process_exit_loop.Run();
  process.Close();
  EXPECT_TRUE(process_exited);
  return exit_code;
}

}  // namespace enterprise_companion
