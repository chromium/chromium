// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/alarm_manager.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

class AlarmManagerTest : public ::testing::Test {
 protected:
  class WallClockDependantTask {
   public:
    WallClockDependantTask() : fired_(false), weak_factory_(this) {}
    base::WeakPtr<WallClockDependantTask> GetWeakPtr() {
      return weak_factory_.GetWeakPtr();
    }
    void OnAlarmFire() { fired_ = true; }
    bool fired_;
    base::WeakPtrFactory<WallClockDependantTask> weak_factory_;
  };

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(AlarmManagerTest, AlarmNotFire) {
  WallClockDependantTask task;
  ASSERT_FALSE(task.fired_);

  // Create the AlarmManager.
  base::Time now = base::Time::Now();
  std::unique_ptr<base::SimpleTestClock> test_clock =
      std::make_unique<base::SimpleTestClock>();
  test_clock->SetNow(now);
  base::SimpleTestClock* clock = test_clock.get();
  std::unique_ptr<AlarmManager> manager = std::make_unique<AlarmManager>(
      clock, base::SingleThreadTaskRunner::GetCurrentDefault());

  base::Time alarm_time = now + base::Minutes(10);
  std::unique_ptr<AlarmHandle> handle(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task.GetWeakPtr()),
      alarm_time));
  task_environment_.FastForwardBy(base::Minutes(9));
  clock->Advance(base::Minutes(9));
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(task.fired_);
}

TEST_F(AlarmManagerTest, AlarmFire) {
  WallClockDependantTask task;
  ASSERT_FALSE(task.fired_);

  // Create the AlarmManager.
  base::Time now = base::Time::Now();
  std::unique_ptr<base::SimpleTestClock> test_clock =
      std::make_unique<base::SimpleTestClock>();
  test_clock->SetNow(now);
  base::SimpleTestClock* clock = test_clock.get();
  std::unique_ptr<AlarmManager> manager = std::make_unique<AlarmManager>(
      clock, base::SingleThreadTaskRunner::GetCurrentDefault());

  // Add an alarm.
  base::Time alarm_time = now + base::Minutes(10);
  std::unique_ptr<AlarmHandle> handle(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task.GetWeakPtr()),
      alarm_time));
  clock->Advance(base::Minutes(10));
  task_environment_.FastForwardBy(base::Minutes(10));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(task.fired_);

  // Fires only once.
  task.fired_ = false;
  clock->Advance(base::Minutes(10));
  task_environment_.FastForwardBy(base::Minutes(10));
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(task.fired_);
}

TEST_F(AlarmManagerTest, AlarmPast) {
  WallClockDependantTask task;
  ASSERT_FALSE(task.fired_);

  // Create the AlarmManager.
  base::Time now = base::Time::Now();
  std::unique_ptr<base::SimpleTestClock> test_clock =
      std::make_unique<base::SimpleTestClock>();
  test_clock->SetNow(now);
  std::unique_ptr<AlarmManager> manager = std::make_unique<AlarmManager>(
      test_clock.get(), base::SingleThreadTaskRunner::GetCurrentDefault());

  // Add an alarm in the past. Should fire right away.
  base::Time alarm_time = base::Time::Now() - base::Minutes(10);
  std::unique_ptr<AlarmHandle> handle(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task.GetWeakPtr()),
      alarm_time));
  task_environment_.FastForwardBy(base::Seconds(10));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(task.fired_);
}

TEST_F(AlarmManagerTest, AlarmTimeJump) {
  WallClockDependantTask task;
  ASSERT_FALSE(task.fired_);

  // Create the AlarmManager.
  base::Time now = base::Time::Now();
  std::unique_ptr<base::SimpleTestClock> test_clock =
      std::make_unique<base::SimpleTestClock>();
  test_clock->SetNow(now);
  base::SimpleTestClock* clock = test_clock.get();
  std::unique_ptr<AlarmManager> manager = std::make_unique<AlarmManager>(
      clock, base::SingleThreadTaskRunner::GetCurrentDefault());

  // Add an alarm. The time jumps to the future.
  base::Time alarm_time = now + base::Minutes(10);
  std::unique_ptr<AlarmHandle> handle(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task.GetWeakPtr()),
      alarm_time));
  clock->Advance(base::Minutes(10));
  task_environment_.FastForwardBy(base::Minutes(1));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(task.fired_);
}

TEST_F(AlarmManagerTest, AlarmJumpFuture) {
  WallClockDependantTask task;
  ASSERT_FALSE(task.fired_);

  // Create the AlarmManager.
  base::Time now = base::Time::Now();
  std::unique_ptr<base::SimpleTestClock> test_clock =
      std::make_unique<base::SimpleTestClock>();
  test_clock->SetNow(now);
  base::SimpleTestClock* clock = test_clock.get();
  std::unique_ptr<AlarmManager> manager = std::make_unique<AlarmManager>(
      clock, base::SingleThreadTaskRunner::GetCurrentDefault());

  // Add an alarm. The time jumps far into the future.
  base::Time alarm_time = now + base::Minutes(10);
  std::unique_ptr<AlarmHandle> handle(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task.GetWeakPtr()),
      alarm_time));
  clock->Advance(base::Minutes(60));
  task_environment_.FastForwardBy(base::Minutes(1));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(task.fired_);
}

TEST_F(AlarmManagerTest, AlarmMultiple) {
  WallClockDependantTask task1;
  WallClockDependantTask task2;
  ASSERT_FALSE(task1.fired_);
  ASSERT_FALSE(task2.fired_);

  // Create the AlarmManager.
  base::Time now = base::Time::Now();
  std::unique_ptr<base::SimpleTestClock> test_clock =
      std::make_unique<base::SimpleTestClock>();
  test_clock->SetNow(now);
  base::SimpleTestClock* clock = test_clock.get();
  std::unique_ptr<AlarmManager> manager = std::make_unique<AlarmManager>(
      clock, base::SingleThreadTaskRunner::GetCurrentDefault());

  // Add first task.
  base::Time alarm_time = now + base::Minutes(10);
  std::unique_ptr<AlarmHandle> handle1(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task1.GetWeakPtr()),
      alarm_time));

  // Add second task.
  alarm_time = now + base::Minutes(12);
  std::unique_ptr<AlarmHandle> handle2(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task2.GetWeakPtr()),
      alarm_time));

  // First task should fire.
  clock->Advance(base::Minutes(10));
  task_environment_.FastForwardBy(base::Minutes(1));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(task1.fired_);
  ASSERT_FALSE(task2.fired_);

  // Reset state;
  task1.fired_ = false;
  task2.fired_ = false;

  // Second task should fire.
  clock->Advance(base::Minutes(2));
  task_environment_.FastForwardBy(base::Minutes(1));
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(task1.fired_);
  ASSERT_TRUE(task2.fired_);
}

TEST_F(AlarmManagerTest, AlarmMultipleReverseOrder) {
  WallClockDependantTask task1;
  WallClockDependantTask task2;
  ASSERT_FALSE(task1.fired_);
  ASSERT_FALSE(task2.fired_);

  // Create the AlarmManager.
  base::Time now = base::Time::Now();
  std::unique_ptr<base::SimpleTestClock> test_clock =
      std::make_unique<base::SimpleTestClock>();
  test_clock->SetNow(now);
  base::SimpleTestClock* clock = test_clock.get();
  std::unique_ptr<AlarmManager> manager = std::make_unique<AlarmManager>(
      clock, base::SingleThreadTaskRunner::GetCurrentDefault());

  // Add first task.
  base::Time alarm_time = now + base::Minutes(12);
  std::unique_ptr<AlarmHandle> handle1(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task1.GetWeakPtr()),
      alarm_time));

  // Add second task.
  alarm_time = now + base::Minutes(10);
  std::unique_ptr<AlarmHandle> handle2(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task2.GetWeakPtr()),
      alarm_time));

  // Second task should fire.
  clock->Advance(base::Minutes(10));
  task_environment_.FastForwardBy(base::Minutes(1));
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(task1.fired_);
  ASSERT_TRUE(task2.fired_);

  // Reset state;
  task1.fired_ = false;
  task2.fired_ = false;

  // First task should fire.
  clock->Advance(base::Minutes(2));
  task_environment_.FastForwardBy(base::Minutes(1));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(task1.fired_);
  ASSERT_FALSE(task2.fired_);
}

TEST_F(AlarmManagerTest, AlarmMultipleSameTime) {
  WallClockDependantTask task1;
  WallClockDependantTask task2;
  WallClockDependantTask task3;
  ASSERT_FALSE(task1.fired_);
  ASSERT_FALSE(task2.fired_);
  ASSERT_FALSE(task3.fired_);

  // Create the AlarmManager.
  base::Time now = base::Time::Now();
  std::unique_ptr<base::SimpleTestClock> test_clock =
      std::make_unique<base::SimpleTestClock>();
  test_clock->SetNow(now);
  base::SimpleTestClock* clock = test_clock.get();
  std::unique_ptr<AlarmManager> manager = std::make_unique<AlarmManager>(
      clock, base::SingleThreadTaskRunner::GetCurrentDefault());

  // Add first task.
  base::Time alarm_time = now + base::Minutes(12);
  std::unique_ptr<AlarmHandle> handle1(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task1.GetWeakPtr()),
      alarm_time));

  // Add second task.
  alarm_time = now + base::Minutes(16);
  std::unique_ptr<AlarmHandle> handle2(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task2.GetWeakPtr()),
      alarm_time));

  // Add third task.
  alarm_time = now + base::Minutes(12);
  std::unique_ptr<AlarmHandle> handle3(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task3.GetWeakPtr()),
      alarm_time));

  // First and third task should fire.
  clock->Advance(base::Minutes(12));
  task_environment_.FastForwardBy(base::Minutes(1));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(task1.fired_);
  ASSERT_FALSE(task2.fired_);
  ASSERT_TRUE(task3.fired_);
}

TEST_F(AlarmManagerTest, AlarmMultipleShuffle) {
  WallClockDependantTask task1;
  WallClockDependantTask task2;
  WallClockDependantTask task3;
  ASSERT_FALSE(task1.fired_);
  ASSERT_FALSE(task2.fired_);
  ASSERT_FALSE(task3.fired_);

  // Create the AlarmManager.
  base::Time now = base::Time::Now();
  std::unique_ptr<base::SimpleTestClock> test_clock =
      std::make_unique<base::SimpleTestClock>();
  test_clock->SetNow(now);
  base::SimpleTestClock* clock = test_clock.get();
  std::unique_ptr<AlarmManager> manager = std::make_unique<AlarmManager>(
      clock, base::SingleThreadTaskRunner::GetCurrentDefault());

  // Add first task.
  base::Time alarm_time = now + base::Minutes(15);
  std::unique_ptr<AlarmHandle> handle1(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task1.GetWeakPtr()),
      alarm_time));

  // Add second task.
  alarm_time = now + base::Minutes(16);
  std::unique_ptr<AlarmHandle> handle2(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task2.GetWeakPtr()),
      alarm_time));

  // Add third task.
  alarm_time = now + base::Minutes(11);
  std::unique_ptr<AlarmHandle> handle3(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task3.GetWeakPtr()),
      alarm_time));

  // Third task should fire.
  clock->Advance(base::Minutes(12));
  task_environment_.FastForwardBy(base::Minutes(1));
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(task1.fired_);
  ASSERT_FALSE(task2.fired_);
  ASSERT_TRUE(task3.fired_);

  clock->Advance(base::Minutes(3));
  task_environment_.FastForwardBy(base::Minutes(1));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(task1.fired_);
  ASSERT_FALSE(task2.fired_);
  ASSERT_TRUE(task3.fired_);
}

TEST_F(AlarmManagerTest, AlarmTwice) {
  WallClockDependantTask task1;
  WallClockDependantTask task2;
  ASSERT_FALSE(task1.fired_);
  ASSERT_FALSE(task2.fired_);

  // Create the AlarmManager.
  base::Time now = base::Time::Now();
  std::unique_ptr<base::SimpleTestClock> test_clock =
      std::make_unique<base::SimpleTestClock>();
  test_clock->SetNow(now);
  base::SimpleTestClock* clock = test_clock.get();
  std::unique_ptr<AlarmManager> manager = std::make_unique<AlarmManager>(
      clock, base::SingleThreadTaskRunner::GetCurrentDefault());

  // Add first task.
  base::Time alarm_time = now + base::Minutes(15);
  std::unique_ptr<AlarmHandle> handle1(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task1.GetWeakPtr()),
      alarm_time));

  // Add it again with less time.
  alarm_time = now + base::Minutes(1);
  std::unique_ptr<AlarmHandle> handle2(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task1.GetWeakPtr()),
      alarm_time));

  // Add second task.
  alarm_time = now + base::Minutes(16);
  std::unique_ptr<AlarmHandle> handle3(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task2.GetWeakPtr()),
      alarm_time));

  // First task should fire.
  clock->Advance(base::Minutes(1));
  task_environment_.FastForwardBy(base::Minutes(1));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(task1.fired_);
  ASSERT_FALSE(task2.fired_);

  task1.fired_ = false;
  task2.fired_ = false;

  // First task should fire again because it was added twice.
  clock->Advance(base::Minutes(14));
  task_environment_.FastForwardBy(base::Minutes(1));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(task1.fired_);
  ASSERT_FALSE(task2.fired_);
}

TEST_F(AlarmManagerTest, AlarmCancel) {
  std::unique_ptr<WallClockDependantTask> task1 =
      std::make_unique<WallClockDependantTask>();
  std::unique_ptr<WallClockDependantTask> task2 =
      std::make_unique<WallClockDependantTask>();
  std::unique_ptr<WallClockDependantTask> task3 =
      std::make_unique<WallClockDependantTask>();
  ASSERT_FALSE(task1->fired_);
  ASSERT_FALSE(task2->fired_);
  ASSERT_FALSE(task3->fired_);

  // Create the AlarmManager.
  base::Time now = base::Time::Now();
  std::unique_ptr<base::SimpleTestClock> test_clock =
      std::make_unique<base::SimpleTestClock>();
  test_clock->SetNow(now);
  base::SimpleTestClock* clock = test_clock.get();
  std::unique_ptr<AlarmManager> manager = std::make_unique<AlarmManager>(
      clock, base::SingleThreadTaskRunner::GetCurrentDefault());

  // Add first task.
  base::Time alarm_time = now + base::Minutes(12);
  std::unique_ptr<AlarmHandle> handle1(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task1->GetWeakPtr()),
      alarm_time));

  // Add second task.
  alarm_time = now + base::Minutes(16);
  std::unique_ptr<AlarmHandle> handle2(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task2->GetWeakPtr()),
      alarm_time));

  // Add third task.
  alarm_time = now + base::Minutes(12);
  std::unique_ptr<AlarmHandle> handle3(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task3->GetWeakPtr()),
      alarm_time));

  // Remove the first task.
  task1.reset(nullptr);

  // Third task should fire.
  clock->Advance(base::Minutes(15));
  task_environment_.FastForwardBy(base::Minutes(1));
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(task2->fired_);
  ASSERT_TRUE(task3->fired_);
}

TEST_F(AlarmManagerTest, AlarmDeleteHandle) {
  std::unique_ptr<WallClockDependantTask> task1 =
      std::make_unique<WallClockDependantTask>();
  std::unique_ptr<WallClockDependantTask> task2 =
      std::make_unique<WallClockDependantTask>();
  std::unique_ptr<WallClockDependantTask> task3 =
      std::make_unique<WallClockDependantTask>();
  ASSERT_FALSE(task1->fired_);
  ASSERT_FALSE(task2->fired_);
  ASSERT_FALSE(task3->fired_);

  // Create the AlarmManager.
  base::Time now = base::Time::Now();
  std::unique_ptr<base::SimpleTestClock> test_clock =
      std::make_unique<base::SimpleTestClock>();
  test_clock->SetNow(now);
  base::SimpleTestClock* clock = test_clock.get();
  std::unique_ptr<AlarmManager> manager = std::make_unique<AlarmManager>(
      clock, base::SingleThreadTaskRunner::GetCurrentDefault());

  // Add first task.
  base::Time alarm_time = now + base::Minutes(12);
  std::unique_ptr<AlarmHandle> handle1(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task1->GetWeakPtr()),
      alarm_time));

  // Add second task.
  alarm_time = now + base::Minutes(16);
  std::unique_ptr<AlarmHandle> handle2(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task2->GetWeakPtr()),
      alarm_time));

  // Add third task.
  alarm_time = now + base::Minutes(12);
  std::unique_ptr<AlarmHandle> handle3(manager->PostAlarmTask(
      base::BindOnce(&WallClockDependantTask::OnAlarmFire, task3->GetWeakPtr()),
      alarm_time));

  // Delete the first task's handle.
  handle1.reset();

  // Third task should fire.
  clock->Advance(base::Minutes(15));
  task_environment_.FastForwardBy(base::Minutes(1));
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(task1->fired_);
  ASSERT_FALSE(task2->fired_);
  ASSERT_TRUE(task3->fired_);
}

}  // namespace chromecast
