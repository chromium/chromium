// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/power_manager_provider_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/libassistant/test_support/fake_platform_delegate.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::libassistant {

namespace {

// Constants to be used with the |AddWakeAlarm| API.
const uint64_t kAlarmRelativeTimeMs = 1000;
const uint64_t kAlarmMaxDelayMs = 0;

class FakePlatformDelegateImpl : public assistant::FakePlatformDelegate {
 public:
  explicit FakePlatformDelegateImpl(
      device::TestWakeLockProvider* wake_lock_provider)
      : wake_lock_provider_(wake_lock_provider) {}

  // FakePlatformDelegate implementation:
  void BindWakeLockProvider(
      mojo::PendingReceiver<::device::mojom::WakeLockProvider> receiver)
      override {
    wake_lock_provider_->BindReceiver(std::move(receiver));
  }

 private:
  const raw_ptr<device::TestWakeLockProvider> wake_lock_provider_;
};

}  // namespace

class AssistantPowerManagerProviderImplTest : public testing::Test {
 public:
  AssistantPowerManagerProviderImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  AssistantPowerManagerProviderImplTest(
      const AssistantPowerManagerProviderImplTest&) = delete;
  AssistantPowerManagerProviderImplTest& operator=(
      const AssistantPowerManagerProviderImplTest&) = delete;

  ~AssistantPowerManagerProviderImplTest() override = default;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::FakePowerManagerClient::Get()->set_tick_clock(
        task_environment_.GetMockTickClock());
    power_manager_provider_impl_ = std::make_unique<PowerManagerProviderImpl>();
    power_manager_provider_impl_->set_tick_clock_for_testing(
        task_environment_.GetMockTickClock());

    power_manager_provider_impl_->Initialize(&platform_delegate_);
  }

  void TearDown() override {
    power_manager_provider_impl_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  PowerManagerProviderImpl* GetPowerManagerProviderImpl() {
    return power_manager_provider_impl_.get();
  }

  // Calls the acquire wake lock API and ensures that the request has reached
  // the wake lock provider.
  void AcquireWakeLock() {
    power_manager_provider_impl_->AcquireWakeLock();
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  // Calls the release wake lock API and ensures that the request has reached
  // the wake lock provider.
  void ReleaseWakeLock() {
    power_manager_provider_impl_->ReleaseWakeLock();
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  // Returns the number of active wake locks of type |type|.
  int GetActiveWakeLocks(device::mojom::WakeLockType type) {
    base::RunLoop run_loop;
    int result_count = 0;
    wake_lock_provider_.GetActiveWakeLocksForTests(
        type,
        base::BindOnce(
            [](base::RunLoop* run_loop, int* result_count, int32_t count) {
              *result_count = count;
              run_loop->Quit();
            },
            &run_loop, &result_count));
    run_loop.Run();
    return result_count;
  }

  // Returns true iff adding a wake alarm via |AddWakeAlarm| returns a > 0
  // result and the alarm expiration callback fires.
  bool CheckAddWakeAlarmAndExpiration(uint64_t relative_time_ms,
                                      uint64_t max_delay_ms) {
    // Schedule wake alarm and check if valid id is returned and timer callback
    // is fired.
    bool result = false;
    assistant_client::Callback0 wake_alarm_expiration_cb(
        [&result]() { result = true; });
    assistant_client::PowerManagerProvider::AlarmId id =
        power_manager_provider_impl_->AddWakeAlarm(
            relative_time_ms, max_delay_ms,
            std::move(wake_alarm_expiration_cb));
    task_environment_.FastForwardBy(base::Milliseconds(relative_time_ms));

    if (id <= 0UL)
      return false;
    return result;
  }

 private:
  // Needs to be of type |MainThreadType::IO| to use |NativeTimer|.
  base::test::TaskEnvironment task_environment_;

  device::TestWakeLockProvider wake_lock_provider_;
  FakePlatformDelegateImpl platform_delegate_{&wake_lock_provider_};

  std::unique_ptr<PowerManagerProviderImpl> power_manager_provider_impl_;
};

TEST_F(AssistantPowerManagerProviderImplTest, CheckAcquireAndReleaseWakeLock) {
  // Acquire wake lock and check wake lock count.
  AcquireWakeLock();
  EXPECT_EQ(1, GetActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventAppSuspension));
  // Acquire another wake lock, this shouldn't change the overall count.
  AcquireWakeLock();
  EXPECT_EQ(1, GetActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventAppSuspension));
  // Release wake lock, this shouldn't change the overall count.
  ReleaseWakeLock();
  EXPECT_EQ(1, GetActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventAppSuspension));
  // Release wake lock, this should finally release the wake lock.
  ReleaseWakeLock();
  EXPECT_EQ(0, GetActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventAppSuspension));
  // An unbalanced release call shouldn't do anything.
  ReleaseWakeLock();
  EXPECT_EQ(0, GetActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventAppSuspension));
}

TEST_F(AssistantPowerManagerProviderImplTest, CheckWakeAlarms) {
  // Check consecutive wake alarms addition and expiration.
  EXPECT_TRUE(
      CheckAddWakeAlarmAndExpiration(kAlarmRelativeTimeMs, kAlarmMaxDelayMs));
  EXPECT_TRUE(
      CheckAddWakeAlarmAndExpiration(kAlarmRelativeTimeMs, kAlarmMaxDelayMs));
}

}  // namespace ash::libassistant
