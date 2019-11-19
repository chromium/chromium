// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/power/native_timer.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class NativeTimerTest : public testing::Test {
 public:
  NativeTimerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~NativeTimerTest() override = default;

  // testing::Test:
  void SetUp() override {
    PowerManagerClient::InitializeFake();
    FakePowerManagerClient::Get()->set_tick_clock(
        task_environment_.GetMockTickClock());
  }

  void TearDown() override { PowerManagerClient::Shutdown(); }

 protected:
  // Returns true iff |Start| on |timer| succeeds and timer expiration occurs
  // too. The underlying fake power manager implementation expires the timer
  bool CheckStartTimerAndExpiration(NativeTimer* timer, base::TimeDelta delay) {
    base::RunLoop start_timer_loop;
    base::RunLoop expiration_loop;
    bool start_timer_result = false;
    bool expiration_result = false;
    timer->Start(task_environment_.GetMockTickClock()->NowTicks() + delay,
                 base::BindOnce([](bool* result_out) { *result_out = true; },
                                &expiration_result),
                 base::BindOnce([](bool* result_out,
                                   bool result) { *result_out = result; },
                                &start_timer_result));

    // Both starting the timer and timer firing should succeed.
    task_environment_.FastForwardBy(delay);
    if (!start_timer_result)
      return false;
    return expiration_result;
  }

  base::test::TaskEnvironment task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeTimerTest);
};

TEST_F(NativeTimerTest, CheckCreateFailure) {
  // Create the timer. It queues async operations; enclose it in a run loop.
  // This should fail internally as an empty tag is provided.
  base::RunLoop create_timer_loop;
  NativeTimer timer("");
  create_timer_loop.RunUntilIdle();

  // Starting the timer should fail as timer creation failed.
  EXPECT_FALSE(CheckStartTimerAndExpiration(
      &timer, base::TimeDelta::FromMilliseconds(1000)));
}

TEST_F(NativeTimerTest, CheckCreateAndStartTimer) {
  // Create the timer. It queues async operations; enclose it in a run loop.
  base::RunLoop create_timer_loop;
  NativeTimer timer("Assistant");
  create_timer_loop.RunUntilIdle();

  // Start timer and check if starting the timer and its expiration succeeded.
  EXPECT_TRUE(CheckStartTimerAndExpiration(
      &timer, base::TimeDelta::FromMilliseconds(1000)));

  // Start another timer and check if starting the timer and its expiration
  // succeeded.
  EXPECT_TRUE(CheckStartTimerAndExpiration(
      &timer, base::TimeDelta::FromMilliseconds(1000)));
}

}  // namespace chromeos
