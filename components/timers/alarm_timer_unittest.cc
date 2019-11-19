// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/timerfd.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/timers/alarm_timer_chromeos.h"
#include "testing/gtest/include/gtest/gtest.h"

// Most of these tests have been lifted right out of timer_unittest.cc with only
// cosmetic changes. We want the AlarmTimer to be a drop-in replacement for the
// regular Timer so it should pass the same tests as the Timer class.
namespace timers {
namespace {

constexpr base::TimeDelta kTenMilliseconds =
    base::TimeDelta::FromMilliseconds(10);

class AlarmTimerTester {
 public:
  AlarmTimerTester(bool* did_run,
                   base::TimeDelta delay,
                   base::OnceClosure quit_closure)
      : did_run_(did_run),
        quit_closure_(std::move(quit_closure)),
        delay_(delay),
        timer_(SimpleAlarmTimer::CreateForTesting()) {}
  void Start() {
    timer_->Start(
        FROM_HERE, delay_,
        base::BindRepeating(&AlarmTimerTester::Run, base::Unretained(this)));
  }

 private:
  void Run() {
    *did_run_ = true;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  bool* did_run_;
  base::OnceClosure quit_closure_;
  const base::TimeDelta delay_;
  std::unique_ptr<SimpleAlarmTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(AlarmTimerTester);
};

class SelfDeletingAlarmTimerTester {
 public:
  SelfDeletingAlarmTimerTester(bool* did_run,
                               base::TimeDelta delay,
                               base::OnceClosure quit_closure)
      : did_run_(did_run),
        quit_closure_(std::move(quit_closure)),
        delay_(delay),
        timer_(SimpleAlarmTimer::CreateForTesting()) {}
  void Start() {
    timer_->Start(FROM_HERE, delay_,
                  base::BindRepeating(&SelfDeletingAlarmTimerTester::Run,
                                      base::Unretained(this)));
  }

 private:
  void Run() {
    *did_run_ = true;
    timer_.reset();

    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  bool* did_run_;
  base::OnceClosure quit_closure_;
  const base::TimeDelta delay_;
  std::unique_ptr<SimpleAlarmTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(SelfDeletingAlarmTimerTester);
};

}  // namespace

//-----------------------------------------------------------------------------
// Each test is run against each type of MessageLoop.  That way we are sure
// that timers work properly in all configurations.

TEST(AlarmTimerTest, SimpleAlarmTimer) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  base::RunLoop run_loop;
  bool did_run = false;
  AlarmTimerTester f(&did_run, kTenMilliseconds,
                     run_loop.QuitWhenIdleClosure());
  f.Start();

  run_loop.Run();

  EXPECT_TRUE(did_run);
}

TEST(AlarmTimerTest, SimpleAlarmTimer_Cancel) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  bool did_run_a = false;
  AlarmTimerTester* a =
      new AlarmTimerTester(&did_run_a, kTenMilliseconds, base::OnceClosure());

  // This should run before the timer expires.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, a);

  // Now start the timer.
  a->Start();

  base::RunLoop run_loop;
  bool did_run_b = false;
  AlarmTimerTester b(&did_run_b, kTenMilliseconds,
                     run_loop.QuitWhenIdleClosure());
  b.Start();

  run_loop.Run();

  EXPECT_FALSE(did_run_a);
  EXPECT_TRUE(did_run_b);
}

// If underlying timer does not handle this properly, we will crash or fail
// in full page heap environment.
TEST(AlarmTimerTest, SelfDeletingAlarmTimer) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  base::RunLoop run_loop;
  bool did_run = false;
  SelfDeletingAlarmTimerTester f(&did_run, kTenMilliseconds,
                                 run_loop.QuitWhenIdleClosure());
  f.Start();

  run_loop.Run();

  EXPECT_TRUE(did_run);
}

TEST(AlarmTimerTest, AlarmTimerZeroDelay) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  base::RunLoop run_loop;
  bool did_run = false;
  AlarmTimerTester f(&did_run, base::TimeDelta(),
                     run_loop.QuitWhenIdleClosure());
  f.Start();

  run_loop.Run();

  EXPECT_TRUE(did_run);
}

TEST(AlarmTimerTest, AlarmTimerZeroDelay_Cancel) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  bool did_run_a = false;
  AlarmTimerTester* a =
      new AlarmTimerTester(&did_run_a, base::TimeDelta(), base::OnceClosure());

  // This should run before the timer expires.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, a);

  // Now start the timer.
  a->Start();

  base::RunLoop run_loop;
  bool did_run_b = false;
  AlarmTimerTester b(&did_run_b, base::TimeDelta(),
                     run_loop.QuitWhenIdleClosure());
  b.Start();

  run_loop.Run();

  EXPECT_FALSE(did_run_a);
  EXPECT_TRUE(did_run_b);
}

TEST(AlarmTimerTest, MessageLoopShutdown) {
  // This test is designed to verify that shutdown of the
  // message loop does not cause crashes if there were pending
  // timers not yet fired.  It may only trigger exceptions
  // if debug heap checking is enabled.
  bool did_run = false;
  {
    base::test::TaskEnvironment task_environment(
        base::test::TaskEnvironment::MainThreadType::IO);

    AlarmTimerTester a(&did_run, kTenMilliseconds, base::OnceClosure());
    AlarmTimerTester b(&did_run, kTenMilliseconds, base::OnceClosure());
    AlarmTimerTester c(&did_run, kTenMilliseconds, base::OnceClosure());
    AlarmTimerTester d(&did_run, kTenMilliseconds, base::OnceClosure());

    a.Start();
    b.Start();

    // Allow FileDescriptorWatcher to start watching the timers. Without this,
    // tasks posted by FileDescriptorWatcher::WatchReadable() are leaked.
    base::RunLoop().RunUntilIdle();

  }  // SimpleAlarmTimers destruct. SHOULD NOT CRASH, of course.

  EXPECT_FALSE(did_run);
}

TEST(AlarmTimerTest, NonRepeatIsRunning) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  auto timer = SimpleAlarmTimer::CreateForTesting();
  EXPECT_FALSE(timer->IsRunning());
  timer->Start(FROM_HERE, base::TimeDelta::FromDays(1), base::DoNothing());

  // Allow FileDescriptorWatcher to start watching the timer. Without this, a
  // task posted by FileDescriptorWatcher::WatchReadable() is leaked.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(timer->IsRunning());
  timer->Stop();
  EXPECT_FALSE(timer->IsRunning());
  ASSERT_FALSE(timer->user_task().is_null());
  timer->Reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(timer->IsRunning());
}

TEST(AlarmTimerTest, RetainNonRepeatIsRunning) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  auto timer = SimpleAlarmTimer::CreateForTesting();
  EXPECT_FALSE(timer->IsRunning());
  timer->Start(FROM_HERE, base::TimeDelta::FromDays(1), base::DoNothing());

  // Allow FileDescriptorWatcher to start watching the timer. Without this, a
  // task posted by FileDescriptorWatcher::WatchReadable() is leaked.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(timer->IsRunning());
  timer->Reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(timer->IsRunning());
  timer->Stop();
  EXPECT_FALSE(timer->IsRunning());
  timer->Reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(timer->IsRunning());
}

namespace {

bool g_callback_happened1 = false;
bool g_callback_happened2 = false;

void ClearAllCallbackHappened() {
  g_callback_happened1 = false;
  g_callback_happened2 = false;
}

void SetCallbackHappened1(base::OnceClosure quit_closure) {
  g_callback_happened1 = true;
  if (quit_closure)
    std::move(quit_closure).Run();
}

void SetCallbackHappened2(base::OnceClosure quit_closure) {
  g_callback_happened2 = true;
  if (quit_closure)
    std::move(quit_closure).Run();
}

TEST(AlarmTimerTest, ContinuationStopStart) {
  ClearAllCallbackHappened();
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  auto timer = SimpleAlarmTimer::CreateForTesting();
  timer->Start(FROM_HERE, base::TimeDelta::FromMilliseconds(10),
               base::BindRepeating(&SetCallbackHappened1,
                                   base::DoNothing().Repeatedly()));
  timer->Stop();

  base::RunLoop run_loop;
  timer->Start(FROM_HERE, base::TimeDelta::FromMilliseconds(40),
               base::BindRepeating(&SetCallbackHappened2,
                                   run_loop.QuitWhenIdleClosure()));
  run_loop.Run();

  EXPECT_FALSE(g_callback_happened1);
  EXPECT_TRUE(g_callback_happened2);
}

TEST(AlarmTimerTest, ContinuationReset) {
  ClearAllCallbackHappened();
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  base::RunLoop run_loop;
  auto timer = SimpleAlarmTimer::CreateForTesting();
  timer->Start(FROM_HERE, base::TimeDelta::FromMilliseconds(10),
               base::BindRepeating(&SetCallbackHappened1,
                                   run_loop.QuitWhenIdleClosure()));
  timer->Reset();
  ASSERT_FALSE(timer->user_task().is_null());
  run_loop.Run();
  EXPECT_TRUE(g_callback_happened1);
}

// Verify that no crash occurs if a timer is deleted while its callback is
// running.
TEST(AlarmTimerTest, DeleteTimerWhileCallbackIsRunning) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  base::RunLoop run_loop;

  // Will be deleted by the callback.
  auto timer = SimpleAlarmTimer::CreateForTesting();
  auto* timer_ptr = timer.get();
  timer_ptr->Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(10),
      base::BindRepeating([](std::unique_ptr<SimpleAlarmTimer> timer,
                             base::RunLoop* run_loop) { run_loop->Quit(); },
                          base::Passed(std::move(timer)), &run_loop));
  run_loop.Run();
}

// Verify that no crash occurs if a zero-delay timer is deleted while its
// callback is running.
TEST(AlarmTimerTest, DeleteTimerWhileCallbackIsRunningZeroDelay) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
  base::RunLoop run_loop;

  // Will be deleted by the callback.
  auto timer = SimpleAlarmTimer::CreateForTesting();
  auto* timer_ptr = timer.get();
  timer_ptr->Start(
      FROM_HERE, base::TimeDelta(),
      base::BindRepeating([](std::unique_ptr<SimpleAlarmTimer> timer,
                             base::RunLoop* run_loop) { run_loop->Quit(); },
                          base::Passed(std::move(timer)), &run_loop));
  run_loop.Run();
}

}  // namespace
}  // namespace timers
