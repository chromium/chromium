// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"

#include <utility>

#include "base/check_op.h"
#include "chrome/chrome_cleaner/ipc/ipc_test_util.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace chrome_cleaner {
namespace {

MULTIPROCESS_TEST_MAIN(MojoTaskRunnerTestClientMain) {
  scoped_refptr<MojoTaskRunner> mojo_task_runner = MojoTaskRunner::Create();
  scoped_refptr<ChildProcess> child_process =
      base::MakeRefCounted<ChildProcess>(mojo_task_runner);

  CHECK_NE(nullptr, mojo_task_runner);
  CHECK_NE(nullptr, mojo::core::GetIOTaskRunner());
  return 0;
}

class MojoTaskRunnerTestParentProcess : public ParentProcess {
 public:
  explicit MojoTaskRunnerTestParentProcess(
      scoped_refptr<MojoTaskRunner> mojo_task_runner)
      : ParentProcess(mojo_task_runner) {}

 protected:
  ~MojoTaskRunnerTestParentProcess() override = default;

  void CreateImpl(mojo::ScopedMessagePipeHandle mojo_pipe) override {}
  void DestroyImpl() override {}
};

TEST(MojoTaskRunnerTest, Setup) {
  // Start the MojoTaskRunner on the parent process and checks if it's valid.
  scoped_refptr<MojoTaskRunner> mojo_task_runner = MojoTaskRunner::Create();
  CHECK_NE(nullptr, mojo_task_runner);
  CHECK_NE(nullptr, mojo::core::GetIOTaskRunner());

  scoped_refptr<MojoTaskRunnerTestParentProcess> parent_process =
      base::MakeRefCounted<MojoTaskRunnerTestParentProcess>(
          std::move(mojo_task_runner));

  int32_t exit_code = -1;
  EXPECT_TRUE(parent_process->LaunchConnectedChildProcess(
      "MojoTaskRunnerTestClientMain", &exit_code));
  EXPECT_EQ(0, exit_code);
}

}  // namespace
}  // namespace chrome_cleaner
