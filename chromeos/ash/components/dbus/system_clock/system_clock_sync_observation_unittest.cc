// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/system_clock/system_clock_sync_observation.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/system_clock/fake_system_clock_client.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
constexpr base::TimeDelta kLongTimeout = base::Seconds(100);
constexpr base::TimeDelta kShortTimeout = base::Microseconds(200);
}  // namespace

class SystemClockSyncObservationTest : public testing::Test {
 public:
  SystemClockSyncObservationTest() {
    // By default the system clock is not available yet.
    fake_system_clock_client_.SetServiceIsAvailable(false);
  }

  ~SystemClockSyncObservationTest() override = default;

  SystemClockSyncObservationTest(const SystemClockSyncObservationTest& other) =
      delete;
  SystemClockSyncObservationTest& operator=(
      const SystemClockSyncObservationTest& other) = delete;

 protected:
  FakeSystemClockClient fake_system_clock_client_;

 private:
  base::test::TaskEnvironment task_environment_;
};

// The system clock service becomes available and the clock is already
// synchronized then.
TEST_F(SystemClockSyncObservationTest,
       SuccessSynchronizedWhenServiceAvailable) {
  base::test::TestFuture<bool> future;
  auto system_clock_sync_observation =
      SystemClockSyncObservation::WaitForSystemClockSync(
          &fake_system_clock_client_, kLongTimeout, future.GetCallback());

  fake_system_clock_client_.SetNetworkSynchronized(true);
  fake_system_clock_client_.SetServiceIsAvailable(true);

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get());
}

// The system clock service is already available and the clock becomes
// synchronized later.
TEST_F(SystemClockSyncObservationTest,
       SuccessSynchronizedAfterServiceAvailable) {
  base::test::TestFuture<bool> future;
  auto system_clock_sync_observation =
      SystemClockSyncObservation::WaitForSystemClockSync(
          &fake_system_clock_client_, kLongTimeout, future.GetCallback());

  // Wait until SystemClockSyncObservation becomes aware that the SystemClock
  // service is available and gets the first response from the SystemClock
  // service that the system clock is not synchronized yet.
  base::RunLoop first_sync_info_arrived_loop;
  system_clock_sync_observation->set_on_last_sync_info_for_testing(
      first_sync_info_arrived_loop.QuitClosure());
  fake_system_clock_client_.SetServiceIsAvailable(true);
  first_sync_info_arrived_loop.Run();

  EXPECT_FALSE(future.IsReady());

  fake_system_clock_client_.SetNetworkSynchronized(true);
  fake_system_clock_client_.NotifyObserversSystemClockUpdated();

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get());
}

// When the object is deleted, the callback is not called anymore.
TEST_F(SystemClockSyncObservationTest, DeletingObjectCancelsWaiting) {
  base::test::TestFuture<bool> future;
  auto system_clock_sync_observation =
      SystemClockSyncObservation::WaitForSystemClockSync(
          &fake_system_clock_client_, kLongTimeout, future.GetCallback());

  system_clock_sync_observation.reset();

  fake_system_clock_client_.SetNetworkSynchronized(true);
  fake_system_clock_client_.SetServiceIsAvailable(true);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(future.IsReady());
}

// Multiple notifications that the system clock has been synchronized arrive.
TEST_F(SystemClockSyncObservationTest,
       MultipleSystemClockSynchronizedNotifications) {
  base::test::TestFuture<bool> future;
  auto system_clock_sync_observation =
      SystemClockSyncObservation::WaitForSystemClockSync(
          &fake_system_clock_client_, kLongTimeout, future.GetCallback());

  fake_system_clock_client_.SetServiceIsAvailable(true);
  fake_system_clock_client_.SetNetworkSynchronized(true);
  fake_system_clock_client_.NotifyObserversSystemClockUpdated();

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get());

  // Another notification arrives. The SystemClockSyncObservation should ignore
  // it an not crash.
  fake_system_clock_client_.NotifyObserversSystemClockUpdated();
  base::RunLoop().RunUntilIdle();
}

// The system clock service does not become available
TEST_F(SystemClockSyncObservationTest, TimeoutWaitingForService) {
  base::test::TestFuture<bool> future;
  auto system_clock_sync_observation =
      SystemClockSyncObservation::WaitForSystemClockSync(
          &fake_system_clock_client_, kShortTimeout, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get());
}

// The system clock service becomes available but the clock is not synchronized.
TEST_F(SystemClockSyncObservationTest, TimeoutWaitingForSync) {
  base::test::TestFuture<bool> future;
  auto system_clock_sync_observation =
      SystemClockSyncObservation::WaitForSystemClockSync(
          &fake_system_clock_client_, kShortTimeout, future.GetCallback());

  fake_system_clock_client_.SetServiceIsAvailable(true);

  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get());
}

}  // namespace ash
