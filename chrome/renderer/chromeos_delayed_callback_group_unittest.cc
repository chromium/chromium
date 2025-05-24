// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chromeos_delayed_callback_group.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
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
  const base::TimeDelta kTimeout = base::Seconds(1);
  base::test::TaskEnvironment task_environment;
  auto callback_group = base::MakeRefCounted<DelayedCallbackGroup>(
      kTimeout, base::SequencedTaskRunner::GetCurrentDefault());

  base::Time time_before_add = base::Time::Now();
  base::test::TestFuture<DelayedCallbackGroup::RunReason> future;
  callback_group->Add(future.GetCallback());
  callback_group->RunAll();
  DelayedCallbackGroup::RunReason reason = future.Get();

  base::TimeDelta delta = base::Time::Now() - time_before_add;
  EXPECT_LT(delta, kTimeout);
  EXPECT_EQ(DelayedCallbackGroup::RunReason::NORMAL, reason);
}

TEST(DelayedCallbackGroup, TimeoutSimple) {
  const base::TimeDelta kTimeout = base::Seconds(1);
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto callback_group = base::MakeRefCounted<DelayedCallbackGroup>(
      kTimeout, base::SequencedTaskRunner::GetCurrentDefault());

  base::test::TestFuture<DelayedCallbackGroup::RunReason> future;
  callback_group->Add(future.GetCallback());
  task_environment.FastForwardBy(kTimeout);

  EXPECT_TRUE(future.IsReady());
  DelayedCallbackGroup::RunReason reason = future.Get();
  EXPECT_EQ(DelayedCallbackGroup::RunReason::TIMEOUT, reason);
}

// Failing on CrOS ASAN: crbug.com/1290874
TEST(DelayedCallbackGroup, DISABLED_TimeoutAndRun) {
  const base::TimeDelta kTimeout = base::Seconds(1);
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto callback_group = base::MakeRefCounted<DelayedCallbackGroup>(
      kTimeout, base::SequencedTaskRunner::GetCurrentDefault());

  base::test::TestFuture<DelayedCallbackGroup::RunReason> future1;
  callback_group->Add(future1.GetCallback());
  task_environment.FastForwardBy(kTimeout + base::Milliseconds(100));
  EXPECT_TRUE(future1.IsReady());

  base::test::TestFuture<DelayedCallbackGroup::RunReason> future2;
  callback_group->Add(future2.GetCallback());
  DelayedCallbackGroup::RunReason reason1 = future1.Get();

  EXPECT_EQ(DelayedCallbackGroup::RunReason::TIMEOUT, reason1);
  EXPECT_FALSE(future2.IsReady());

  callback_group->RunAll();
  DelayedCallbackGroup::RunReason reason2 = future2.Get();

  EXPECT_EQ(DelayedCallbackGroup::RunReason::NORMAL, reason2);
}

TEST(DelayedCallbackGroup, DoubleExpiration) {
  const base::TimeDelta kTimeout = base::Seconds(1);
  const base::TimeDelta kTimeDiff = base::Milliseconds(100);
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto callback_group = base::MakeRefCounted<DelayedCallbackGroup>(
      kTimeout, base::SequencedTaskRunner::GetCurrentDefault());

  base::test::TestFuture<DelayedCallbackGroup::RunReason> future1;
  callback_group->Add(future1.GetCallback());
  task_environment.FastForwardBy(kTimeDiff);
  base::test::TestFuture<DelayedCallbackGroup::RunReason> future2;
  callback_group->Add(future2.GetCallback());
  task_environment.FastForwardBy(kTimeout - kTimeDiff);

  EXPECT_TRUE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());
  DelayedCallbackGroup::RunReason reason1 = future1.Get();
  EXPECT_EQ(DelayedCallbackGroup::RunReason::TIMEOUT, reason1);

  task_environment.FastForwardBy(kTimeDiff);

  EXPECT_TRUE(future2.IsReady());
  DelayedCallbackGroup::RunReason reason2 = future2.Get();
  EXPECT_EQ(DelayedCallbackGroup::RunReason::TIMEOUT, reason2);
}
