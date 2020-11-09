// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chromeos_delayed_callback_group.h"

#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

TEST(DelayedCallbackGroup, RunEmpty) {
  base::test::TaskEnvironment task_environment;
  auto callback_group = base::MakeRefCounted<DelayedCallbackGroup>(
      TimeDelta::FromSeconds(1), base::SequencedTaskRunnerHandle::Get());
  callback_group->RunAll();
}

TEST(DelayedCallbackGroup, RunSimple) {
  const TimeDelta kTimeout = TimeDelta::FromMilliseconds(500);
  base::test::TaskEnvironment task_environment;
  auto callback_group = base::MakeRefCounted<DelayedCallbackGroup>(
      TimeDelta::FromSeconds(1), base::SequencedTaskRunnerHandle::Get());

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

  TimeDelta delta = callback_time - time_before_add;
  EXPECT_LT(delta, kTimeout);
}

TEST(DelayedCallbackGroup, TimeoutSimple) {
  const TimeDelta kTimeout = TimeDelta::FromMilliseconds(500);
  base::test::TaskEnvironment task_environment;
  auto callback_group = base::MakeRefCounted<DelayedCallbackGroup>(
      TimeDelta::FromSeconds(1), base::SequencedTaskRunnerHandle::Get());

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

  TimeDelta delta = callback_time - time_before_add;
  EXPECT_GE(delta, kTimeout);
}

TEST(DelayedCallbackGroup, TimeoutAndRun) {
  const TimeDelta kTimeout = TimeDelta::FromMilliseconds(500);
  base::test::TaskEnvironment task_environment;
  auto callback_group = base::MakeRefCounted<DelayedCallbackGroup>(
      TimeDelta::FromSeconds(1), base::SequencedTaskRunnerHandle::Get());

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
  base::PlatformThread::Sleep(kTimeout + TimeDelta::FromMilliseconds(100));
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

  TimeDelta delta = callback_time_1 - start_time;
  EXPECT_GE(delta, kTimeout);
  // Only the first callback should have timed out.
  EXPECT_TRUE(callback_time_2.is_null());
  callback_group->RunAll();
  run_loop_2.Run();
  delta = callback_time_2 - start_time;
  EXPECT_GE(delta, kTimeout + TimeDelta::FromMilliseconds(100));
}

TEST(DelayedCallbackGroup, DoubleExpiration) {
  const TimeDelta kTimeout = TimeDelta::FromMilliseconds(500);
  base::test::TaskEnvironment task_environment;
  auto callback_group = base::MakeRefCounted<DelayedCallbackGroup>(
      TimeDelta::FromSeconds(1), base::SequencedTaskRunnerHandle::Get());

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
  base::PlatformThread::Sleep(TimeDelta::FromMilliseconds(100));
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

  TimeDelta delta = callback_time_1 - start_time;
  EXPECT_GE(delta, kTimeout);
  // Only the first callback should have timed out.
  EXPECT_TRUE(callback_time_2.is_null());
  run_loop_2.Run();
  delta = callback_time_2 - start_time;
  EXPECT_GE(delta, kTimeout + TimeDelta::FromMilliseconds(100));
}
