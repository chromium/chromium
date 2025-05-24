// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/atomic_boolean.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

class AtomicBooleanTest : public testing::Test {
 protected:
  void VerifyAtomicBooleanOnThread(bool value) {
    base::RunLoop run_loop;
    auto callback = base::BindLambdaForTesting([&, value]() {
      VerifyAtomicBoolean(value);
      run_loop.Quit();
    });
    task_runner_->PostTask(FROM_HERE, std::move(callback));

    run_loop.Run();
  }

  void SetAtomicBooleanOnThread(bool value) {
    base::RunLoop run_loop;
    auto callback = base::BindLambdaForTesting([&, value]() {
      SetAtomicBoolean(value);
      run_loop.Quit();
    });
    task_runner_->PostTask(FROM_HERE, std::move(callback));

    run_loop.Run();
  }

  void VerifyAtomicBoolean(bool value) { EXPECT_EQ(value, GetAtomicBoolean()); }

  void SetAtomicBoolean(bool value) { atomic_boolean_.Set(value); }

  bool GetAtomicBoolean() { return atomic_boolean_.Get(); }

  base::test::TaskEnvironment task_environment_;

 private:
  AtomicBoolean atomic_boolean_{false};
  scoped_refptr<base::TaskRunner> task_runner_ =
      base::ThreadPool::CreateTaskRunner({base::MayBlock()});
};

TEST_F(AtomicBooleanTest, SetOnSameThread) {
  VerifyAtomicBoolean(false);

  SetAtomicBoolean(true);
  VerifyAtomicBoolean(true);
}

TEST_F(AtomicBooleanTest, MultipleSetGetOnSameThread) {
  VerifyAtomicBoolean(false);

  SetAtomicBoolean(true);
  VerifyAtomicBoolean(true);

  SetAtomicBoolean(true);
  VerifyAtomicBoolean(true);

  SetAtomicBoolean(false);
  VerifyAtomicBoolean(false);

  SetAtomicBoolean(true);
  VerifyAtomicBoolean(true);
}

TEST_F(AtomicBooleanTest, SetOnNewThread) {
  VerifyAtomicBoolean(false);

  SetAtomicBooleanOnThread(true);
  VerifyAtomicBoolean(true);
}

TEST_F(AtomicBooleanTest, GetOnNewThread) {
  VerifyAtomicBoolean(false);

  SetAtomicBoolean(true);
  VerifyAtomicBoolean(true);

  VerifyAtomicBooleanOnThread(true);
}

}  // namespace nearby::chrome
