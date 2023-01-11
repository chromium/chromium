// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/thread_health_checker.h"

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

namespace {
const base::TimeDelta kInterval = base::Seconds(3);
const base::TimeDelta kTimeout = base::Seconds(2);
}  // namespace

class ThreadHealthCheckerTest : public ::testing::Test {
 protected:
  ThreadHealthCheckerTest()
      : patient_(base::MakeRefCounted<base::TestMockTimeTaskRunner>()),
        doctor_(base::MakeRefCounted<base::TestMockTimeTaskRunner>()),
        event_(base::WaitableEvent::ResetPolicy::MANUAL,
               base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  ~ThreadHealthCheckerTest() override {}

  scoped_refptr<base::TestMockTimeTaskRunner> patient_;
  scoped_refptr<base::TestMockTimeTaskRunner> doctor_;
  base::WaitableEvent event_;
};

#define CREATE_THREAD_HEALTH_CHECKER(name)                                   \
  ThreadHealthChecker name(patient_, doctor_, kInterval, kTimeout,           \
                           base::BindRepeating(&base::WaitableEvent::Signal, \
                                               base::Unretained(&event_)))

TEST_F(ThreadHealthCheckerTest, FiresTimeoutWhenTaskRunnerDoesNotFlush) {
  CREATE_THREAD_HEALTH_CHECKER(thc);
  // Do not flush the patient, so that the health check sentinel task won't run.
  doctor_->FastForwardBy(base::Seconds(6));
  EXPECT_TRUE(event_.IsSignaled());
}

TEST_F(ThreadHealthCheckerTest, DoesNotFireTimeoutWhenTaskRunnerFlushesInTime) {
  CREATE_THREAD_HEALTH_CHECKER(thc);
  // Advance the doctor by enough time to post the health check, but not to time
  // out.
  doctor_->FastForwardBy(base::Seconds(4));
  // Advance the patient to let the sentinel task run.
  patient_->FastForwardBy(base::Seconds(4));
  // Advance the doctor by enough time such that the sentinel not running would
  // cause the failure callback to run.
  doctor_->FastForwardBy(base::Seconds(2));
  EXPECT_FALSE(event_.IsSignaled());
}

TEST_F(ThreadHealthCheckerTest, FiresTimeoutWhenTaskRunnerFlushesTooLate) {
  CREATE_THREAD_HEALTH_CHECKER(thc);
  // Advance the doctor before the patient, to simulate a task in the patient
  // that takes too long.
  doctor_->FastForwardBy(base::Seconds(6));
  patient_->FastForwardBy(base::Seconds(6));
  // Flush the task runner so the health check sentinel task is run.
  EXPECT_TRUE(event_.IsSignaled());
}

TEST_F(ThreadHealthCheckerTest, FiresTimeoutOnLaterIteration) {
  CREATE_THREAD_HEALTH_CHECKER(thc);
  // Advance the doctor enough to start the check.
  doctor_->FastForwardBy(base::Seconds(4));
  // Advance the patient enough to run the task.
  patient_->FastForwardBy(base::Seconds(4));
  // Advance the doctor enough to start the check again.
  doctor_->FastForwardBy(base::Seconds(4));
  EXPECT_FALSE(event_.IsSignaled());
  // Advance the doctor enough for the timeout from the second check to fire.
  doctor_->FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(event_.IsSignaled());
}

TEST_F(ThreadHealthCheckerTest, NoCrashWhenDestroyed) {
  {
    CREATE_THREAD_HEALTH_CHECKER(thc);
    doctor_->RunUntilIdle();
  }
  doctor_->RunUntilIdle();
}

TEST_F(ThreadHealthCheckerTest, DropPendingEventsAfterDestruction) {
  {
    CREATE_THREAD_HEALTH_CHECKER(thc);
    // Fast forward by enough time to have scheduled a health check.
    doctor_->FastForwardBy(base::Seconds(4));
    EXPECT_FALSE(event_.IsSignaled());
  }
  // Fast forward by enough time for the health check to have executed.
  // However, we want all pending events to be dropped after the destructor is
  // called, so the event should not be signalled.
  doctor_->FastForwardBy(base::Seconds(2));
  EXPECT_FALSE(event_.IsSignaled());
}

}  // namespace chromecast
