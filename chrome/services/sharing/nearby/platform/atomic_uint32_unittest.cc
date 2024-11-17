// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/atomic_uint32.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

class AtomicUint32Test : public testing::Test {
 protected:
  void VerifyValueInThread(std::uint32_t value) {
    base::RunLoop run_loop;
    auto callback = base::BindLambdaForTesting([&, value]() {
      VerifyValue(value);
      run_loop.Quit();
    });
    task_runner_->PostTask(FROM_HERE, std::move(callback));

    run_loop.Run();
  }

  void SetValueInThread(std::uint32_t value) {
    base::RunLoop run_loop;
    auto callback = base::BindLambdaForTesting([&, value]() {
      SetValue(value);
      run_loop.Quit();
    });
    task_runner_->PostTask(FROM_HERE, std::move(callback));

    run_loop.Run();
  }

  void VerifyValue(std::uint32_t value) { EXPECT_EQ(value, GetValue()); }

  void SetValue(std::uint32_t value) { atomic_reference_.Set(value); }

  std::uint32_t GetValue() { return atomic_reference_.Get(); }

  std::uint32_t initial_value_ = 8964;
  base::test::TaskEnvironment task_environment_;

 private:
  AtomicUint32 atomic_reference_{initial_value_};
  scoped_refptr<base::TaskRunner> task_runner_ =
      base::ThreadPool::CreateTaskRunner({base::MayBlock()});
};

TEST_F(AtomicUint32Test, GetOnSameThread) {
  VerifyValue(initial_value_);
}

TEST_F(AtomicUint32Test, SetGetOnSameThread) {
  std::uint32_t new_token = 51;
  SetValue(new_token);
  VerifyValue(new_token);
}

TEST_F(AtomicUint32Test, SetOnNewThread) {
  std::uint32_t new_thread_token = 51;
  SetValueInThread(new_thread_token);
  VerifyValue(new_thread_token);
}

TEST_F(AtomicUint32Test, GetOnNewThread) {
  std::uint32_t new_token = 51;
  SetValue(new_token);
  VerifyValueInThread(new_token);
}

}  // namespace nearby::chrome
