// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/bind_to_task_runner.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/free_deleter.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

void BoundBoolSet(bool* var, bool val) {
  *var = val;
}

void BoundBoolSetFromScopedPtr(bool* var, std::unique_ptr<bool> val) {
  *var = *val;
}

void BoundBoolSetFromScopedPtrFreeDeleter(
    bool* var,
    std::unique_ptr<bool, base::FreeDeleter> val) {
  *var = *val;
}

void BoundBoolSetFromScopedArray(bool* var, std::unique_ptr<bool[]> val) {
  *var = val[0];
}

void BoundBoolSetFromConstRef(bool* var, const bool& val) {
  *var = val;
}

void BoundIntegersSet(int* a_var, int* b_var, int a_val, int b_val) {
  *a_var = a_val;
  *b_var = b_val;
}

// Various tests that check that the bound function is only actually executed
// on the message loop, not during the original Run.
class BindToTaskRunnerTest : public ::testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(BindToTaskRunnerTest, Closure) {
  // Test the closure is run inside the loop, not outside it.
  base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::RepeatingClosure cb = BindToCurrentSequence(
      base::BindRepeating(&base::WaitableEvent::Signal, Unretained(&waiter)));
  cb.Run();
  EXPECT_FALSE(waiter.IsSignaled());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(waiter.IsSignaled());
}

TEST_F(BindToTaskRunnerTest, Bool) {
  bool bool_var = false;
  base::RepeatingCallback<void(bool)> cb =
      BindToCurrentSequence(base::BindRepeating(&BoundBoolSet, &bool_var));
  cb.Run(true);
  EXPECT_FALSE(bool_var);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_var);
}

TEST_F(BindToTaskRunnerTest, BoundScopedPtrBool) {
  bool bool_val = false;
  std::unique_ptr<bool> scoped_ptr_bool(new bool(true));
  base::RepeatingClosure cb = BindToCurrentSequence(base::BindRepeating(
      &BoundBoolSetFromScopedPtr, &bool_val, base::Passed(&scoped_ptr_bool)));
  cb.Run();
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToTaskRunnerTest, PassedScopedPtrBool) {
  bool bool_val = false;
  std::unique_ptr<bool> scoped_ptr_bool(new bool(true));
  base::RepeatingCallback<void(std::unique_ptr<bool>)> cb =
      BindToCurrentSequence(
          base::BindRepeating(&BoundBoolSetFromScopedPtr, &bool_val));
  cb.Run(std::move(scoped_ptr_bool));
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToTaskRunnerTest, BoundScopedArrayBool) {
  bool bool_val = false;
  std::unique_ptr<bool[]> scoped_array_bool(new bool[1]);
  scoped_array_bool[0] = true;
  base::RepeatingClosure cb = BindToCurrentSequence(
      base::BindRepeating(&BoundBoolSetFromScopedArray, &bool_val,
                          base::Passed(&scoped_array_bool)));
  cb.Run();
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToTaskRunnerTest, PassedScopedArrayBool) {
  bool bool_val = false;
  std::unique_ptr<bool[]> scoped_array_bool(new bool[1]);
  scoped_array_bool[0] = true;
  base::RepeatingCallback<void(std::unique_ptr<bool[]>)> cb =
      BindToCurrentSequence(
          base::BindRepeating(&BoundBoolSetFromScopedArray, &bool_val));
  cb.Run(std::move(scoped_array_bool));
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToTaskRunnerTest, BoundScopedPtrFreeDeleterBool) {
  bool bool_val = false;
  std::unique_ptr<bool, base::FreeDeleter> scoped_ptr_free_deleter_bool(
      static_cast<bool*>(malloc(sizeof(bool))));
  *scoped_ptr_free_deleter_bool = true;
  base::RepeatingClosure cb = BindToCurrentSequence(
      base::BindRepeating(&BoundBoolSetFromScopedPtrFreeDeleter, &bool_val,
                          base::Passed(&scoped_ptr_free_deleter_bool)));
  cb.Run();
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToTaskRunnerTest, PassedScopedPtrFreeDeleterBool) {
  bool bool_val = false;
  std::unique_ptr<bool, base::FreeDeleter> scoped_ptr_free_deleter_bool(
      static_cast<bool*>(malloc(sizeof(bool))));
  *scoped_ptr_free_deleter_bool = true;
  base::RepeatingCallback<void(std::unique_ptr<bool, base::FreeDeleter>)> cb =
      BindToCurrentSequence(base::BindRepeating(
          &BoundBoolSetFromScopedPtrFreeDeleter, &bool_val));
  cb.Run(std::move(scoped_ptr_free_deleter_bool));
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToTaskRunnerTest, BoolConstRef) {
  bool bool_var = false;
  bool true_var = true;
  const bool& true_ref = true_var;
  base::RepeatingClosure cb = BindToCurrentSequence(
      base::BindRepeating(&BoundBoolSetFromConstRef, &bool_var, true_ref));
  cb.Run();
  EXPECT_FALSE(bool_var);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_var);
}

TEST_F(BindToTaskRunnerTest, Integers) {
  int a = 0;
  int b = 0;
  base::RepeatingCallback<void(int, int)> cb =
      BindToCurrentSequence(base::BindRepeating(&BoundIntegersSet, &a, &b));
  cb.Run(1, -1);
  EXPECT_EQ(a, 0);
  EXPECT_EQ(b, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(a, 1);
  EXPECT_EQ(b, -1);
}

}  // namespace syncer
