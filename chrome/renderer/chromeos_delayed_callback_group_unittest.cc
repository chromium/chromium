// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chromeos_delayed_callback_group.h"

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(DelayedCallbackGroup, RunEmpty) {
  base::test::TaskEnvironment task_environment;
  auto callback_group = base::MakeRefCounted<DelayedCallbackGroup>(
      base::Seconds(1), base::SequencedTaskRunner::GetCurrentDefault());
  callback_group->RunAll();
}

TEST(DelayedCallbackGroup, RunSimple) {
  const base::TimeDelta kTimeout = base::Milliseconds(500);
  base::test::TaskEnvironment task_environment;
  auto callback_group = base::MakeRefCounted<DelayedCallbackGroup>(
      base::Seconds(1), base::SequencedTaskRunner::GetCurrentDefault());

  base::Time time_before_add = base::Time::Now();
  base::Time callback_time;
  base::RunLoop run_loop;
  callback_group->Add(
      base::BindLambdaForTesting([&](DelayedCallbackGroup::RunReason reason) {
        callback_time = base::Time::Now();
        EXPECT_EQ(DelayedCallbackGroup::RunReason::NORMAL, reason);
        run_loop.Quit();
      }));
  callback_group->RunAll();
  run_loop.Run();

  base::TimeDelta delta = callback_time - time_before_add;
  EXPECT_LT(delta, kTimeout);
}

TEST(DelayedCallbackGroup, TimeoutSimple) {
  const base::TimeDelta kTimeout = base::Milliseconds(500);
  base::test::TaskEnvironment task_environment;
  auto callback_group = base::MakeRefCounted<DelayedCallbackGroup>(
      base::Seconds(1), base::SequencedTaskRunner::GetCurrentDefault());

  base::Time time_before_add = base::Time::Now();
  base::Time callback_time;
  base::RunLoop run_loop;
  callback_group->Add(
      base::BindLambdaForTesting([&](DelayedCallbackGroup::RunReason reason) {
        callback_time = base::Time::Now();
        EXPECT_EQ(DelayedCallbackGroup::RunReason::TIMEOUT, reason);
        run_loop.Quit();
      }));
  run_loop.Run();

  base::TimeDelta delta = callback_time - time_before_add;
  EXPECT_GE(delta, kTimeout);
}

#if BUILDFLAG(IS_CHROMEOS)
// Failing on CrOS ASAN: crbug.com/1290874
#define MAYBE_TimeoutAndRun DISABLED_TimeoutAndRun
#else
#define MAYBE_TimeoutAndRun TimeoutAndRun
#endif


TEST(DelayedCallbackGroup, MAYBE_TimeoutAndRun) {
  const base::TimeDelta kTimeout = base::Milliseconds(500);
  base::test::TaskEnvironment task_environment;
  auto callback_group = base::MakeRefCounted<DelayedCallbackGroup>(
      base::Seconds(1), base::SequencedTaskRunner::GetCurrentDefault());

  base::Time start_time = base::Time::Now();
  base::Time callback_time_1;
  base::Time callback_time_2;
  base::RunLoop run_loop_1;
  bool callback_1_called = false;
  callback_group->Add(
      base::BindLambdaForTesting([&](DelayedCallbackGroup::RunReason reason) {
        EXPECT_FALSE(callback_1_called);
        callback_1_called = true;
        callback_time_1 = base::Time::Now();
        EXPECT_EQ(DelayedCallbackGroup::RunReason::TIMEOUT, reason);
        run_loop_1.Quit();
      }));
  base::PlatformThread::Sleep(kTimeout + base::Milliseconds(100));
  base::RunLoop run_loop_2;
  bool callback_2_called = false;
  callback_group->Add(
      base::BindLambdaForTesting([&](DelayedCallbackGroup::RunReason reason) {
        EXPECT_FALSE(callback_2_called);
        callback_2_called = true;
        callback_time_2 = base::Time::Now();
        EXPECT_EQ(DelayedCallbackGroup::RunReason::NORMAL, reason);
        run_loop_2.Quit();
      }));
  run_loop_1.Run();

  base::TimeDelta delta = callback_time_1 - start_time;
  EXPECT_GE(delta, kTimeout);
  // Only the first callback should have timed out.
  EXPECT_TRUE(callback_time_2.is_null());
  callback_group->RunAll();
  run_loop_2.Run();
  delta = callback_time_2 - start_time;
  EXPECT_GE(delta, kTimeout + base::Milliseconds(100));
}

TEST(DelayedCallbackGroup, DoubleExpiration) {
  const base::TimeDelta kTimeout = base::Milliseconds(500);
  base::test::TaskEnvironment task_environment;
  auto callback_group = base::MakeRefCounted<DelayedCallbackGroup>(
      base::Seconds(1), base::SequencedTaskRunner::GetCurrentDefault());

  base::Time start_time = base::Time::Now();
  base::Time callback_time_1;
  base::Time callback_time_2;
  base::RunLoop run_loop_1;
  bool callback_1_called = false;
  callback_group->Add(
      base::BindLambdaForTesting([&](DelayedCallbackGroup::RunReason reason) {
        EXPECT_FALSE(callback_1_called);
        callback_1_called = true;
        callback_time_1 = base::Time::Now();
        EXPECT_EQ(DelayedCallbackGroup::RunReason::TIMEOUT, reason);
        run_loop_1.Quit();
      }));
  base::PlatformThread::Sleep(base::Milliseconds(100));
  base::RunLoop run_loop_2;
  bool callback_2_called = false;
  callback_group->Add(
      base::BindLambdaForTesting([&](DelayedCallbackGroup::RunReason reason) {
        EXPECT_FALSE(callback_2_called);
        callback_2_called = true;
        callback_time_2 = base::Time::Now();
        EXPECT_EQ(DelayedCallbackGroup::RunReason::TIMEOUT, reason);
        run_loop_2.Quit();
      }));
  run_loop_1.Run();

  base::TimeDelta delta = callback_time_1 - start_time;
  EXPECT_GE(delta, kTimeout);
  // Only the first callback should have timed out.
  EXPECT_TRUE(callback_time_2.is_null());
  run_loop_2.Run();
  delta = callback_time_2 - start_time;
  EXPECT_GE(delta, kTimeout + base::Milliseconds(100));
}
