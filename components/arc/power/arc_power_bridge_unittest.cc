// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/power/arc_power_bridge.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/arc/mojom/power.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_power_instance.h"
#include "content/public/common/service_manager_connection.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

using device::mojom::WakeLockType;

class ArcPowerBridgeTest : public testing::Test {
 public:
  // Initial screen brightness percent for the Chrome OS power manager.
  static constexpr double kInitialBrightness = 100.0;

  ArcPowerBridgeTest() = default;
  ~ArcPowerBridgeTest() override = default;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    power_manager_client()->set_screen_brightness_percent(kInitialBrightness);

    wake_lock_provider_ = std::make_unique<device::TestWakeLockProvider>(
        connector_factory_.RegisterInstance(device::mojom::kServiceName));

    bridge_service_ = std::make_unique<ArcBridgeService>();
    power_bridge_ = std::make_unique<ArcPowerBridge>(nullptr /* context */,
                                                     bridge_service_.get());
    power_bridge_->set_connector_for_test(
        connector_factory_.GetDefaultConnector());
    CreatePowerInstance();
  }

  void TearDown() override {
    DestroyPowerInstance();
    power_bridge_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  // Creates a FakePowerInstance for |bridge_service_|. This results in
  // ArcPowerBridge::OnInstanceReady() being called.
  void CreatePowerInstance() {
    power_instance_ = std::make_unique<FakePowerInstance>();
    bridge_service_->power()->SetInstance(power_instance_.get());
    WaitForInstanceReady(bridge_service_->power());
  }

  // Destroys the FakePowerInstance. This results in
  // ArcPowerBridge::OnInstanceClosed() being called.
  void DestroyPowerInstance() {
    if (!power_instance_)
      return;
    bridge_service_->power()->CloseInstance(power_instance_.get());
    power_instance_.reset();
  }

  // Acquires or releases a display wake lock of type |type|.
  void AcquireDisplayWakeLock(mojom::DisplayWakeLockType type) {
    power_bridge_->OnAcquireDisplayWakeLock(type);
    power_bridge_->FlushWakeLocksForTesting();
  }

  void ReleaseDisplayWakeLock(mojom::DisplayWakeLockType type) {
    power_bridge_->OnReleaseDisplayWakeLock(type);
    power_bridge_->FlushWakeLocksForTesting();
  }

  // Returns the number of active wake locks of type |type|.
  int GetActiveWakeLocks(WakeLockType type) {
    base::RunLoop run_loop;
    int result_count = 0;
    wake_lock_provider_->GetActiveWakeLocksForTests(
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

  chromeos::FakePowerManagerClient* power_manager_client() {
    return chromeos::FakePowerManagerClient::Get();
  }

  base::test::TaskEnvironment task_environment_;

  service_manager::TestConnectorFactory connector_factory_;

  std::unique_ptr<ArcBridgeService> bridge_service_;
  std::unique_ptr<FakePowerInstance> power_instance_;
  std::unique_ptr<ArcPowerBridge> power_bridge_;

 private:
  std::unique_ptr<device::TestWakeLockProvider> wake_lock_provider_;

  DISALLOW_COPY_AND_ASSIGN(ArcPowerBridgeTest);
};

TEST_F(ArcPowerBridgeTest, SuspendAndResume) {
  ASSERT_EQ(0, power_instance_->num_suspend());
  ASSERT_EQ(0, power_instance_->num_resume());

  // When powerd notifies Chrome that the system is about to suspend,
  // ArcPowerBridge should notify Android and take a suspend readiness callback
  // to defer the suspend operation.
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, power_instance_->num_suspend());
  EXPECT_EQ(0, power_instance_->num_resume());
  EXPECT_EQ(1,
            power_manager_client()->num_pending_suspend_readiness_callbacks());

  // Simulate Android acknowledging that it's ready for the system to suspend.
  power_instance_->GetSuspendCallback().Run();
  EXPECT_EQ(0,
            power_manager_client()->num_pending_suspend_readiness_callbacks());

  power_manager_client()->SendSuspendDone();
  EXPECT_EQ(1, power_instance_->num_suspend());
  EXPECT_EQ(1, power_instance_->num_resume());

  // We shouldn't crash if the instance isn't ready.
  DestroyPowerInstance();
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(0,
            power_manager_client()->num_pending_suspend_readiness_callbacks());
  power_manager_client()->SendSuspendDone();
}

TEST_F(ArcPowerBridgeTest, SetInteractive) {
  power_bridge_->OnPowerStateChanged(chromeos::DISPLAY_POWER_ALL_OFF);
  EXPECT_FALSE(power_instance_->interactive());

  power_bridge_->OnPowerStateChanged(chromeos::DISPLAY_POWER_ALL_ON);
  EXPECT_TRUE(power_instance_->interactive());

  // We shouldn't crash if the instance isn't ready.
  DestroyPowerInstance();
  power_bridge_->OnPowerStateChanged(chromeos::DISPLAY_POWER_ALL_OFF);
}

TEST_F(ArcPowerBridgeTest, ScreenBrightness) {
  // Let the initial GetScreenBrightnessPercent() task run.
  base::RunLoop().RunUntilIdle();
  EXPECT_DOUBLE_EQ(kInitialBrightness, power_instance_->screen_brightness());

  // Check that Chrome OS brightness changes are passed to Android.
  const double kUpdatedBrightness = 45.0;
  power_manager_client()->set_screen_brightness_percent(kUpdatedBrightness);
  power_manager::BacklightBrightnessChange change;
  change.set_percent(kUpdatedBrightness);
  change.set_cause(power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  power_manager_client()->SendScreenBrightnessChanged(change);
  EXPECT_DOUBLE_EQ(kUpdatedBrightness, power_instance_->screen_brightness());

  // Requests from Android should update the Chrome OS brightness.
  const double kAndroidBrightness = 70.0;
  power_bridge_->OnScreenBrightnessUpdateRequest(kAndroidBrightness);
  EXPECT_DOUBLE_EQ(kAndroidBrightness,
                   power_manager_client()->screen_brightness_percent());

  // To prevent battles between Chrome OS and Android, the updated brightness
  // shouldn't be passed to Android immediately, but it should be passed after
  // the timer fires.
  change.set_percent(kAndroidBrightness);
  power_manager_client()->SendScreenBrightnessChanged(change);
  EXPECT_DOUBLE_EQ(kUpdatedBrightness, power_instance_->screen_brightness());
  ASSERT_TRUE(power_bridge_->TriggerNotifyBrightnessTimerForTesting());
  EXPECT_DOUBLE_EQ(kAndroidBrightness, power_instance_->screen_brightness());
}

TEST_F(ArcPowerBridgeTest, PowerSupplyInfoChanged) {
  base::Optional<power_manager::PowerSupplyProperties> prop =
      power_manager_client()->GetLastStatus();
  ASSERT_TRUE(prop.has_value());
  prop->set_battery_state(power_manager::PowerSupplyProperties::FULL);
  power_manager_client()->UpdatePowerProperties(prop.value());

  // Check that Chrome OS power changes are passed to Android.
  const int prev_call_count = power_instance_->num_power_supply_info();
  prop->set_battery_state(power_manager::PowerSupplyProperties::DISCHARGING);
  power_manager_client()->UpdatePowerProperties(prop.value());
  EXPECT_EQ(1 + prev_call_count, power_instance_->num_power_supply_info());
}

TEST_F(ArcPowerBridgeTest, DifferentWakeLocks) {
  AcquireDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(0,
            GetActiveWakeLocks(WakeLockType::kPreventDisplaySleepAllowDimming));

  AcquireDisplayWakeLock(mojom::DisplayWakeLockType::DIM);
  EXPECT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(1,
            GetActiveWakeLocks(WakeLockType::kPreventDisplaySleepAllowDimming));

  ReleaseDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(0, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(1,
            GetActiveWakeLocks(WakeLockType::kPreventDisplaySleepAllowDimming));

  ReleaseDisplayWakeLock(mojom::DisplayWakeLockType::DIM);
  EXPECT_EQ(0, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(0,
            GetActiveWakeLocks(WakeLockType::kPreventDisplaySleepAllowDimming));
}

TEST_F(ArcPowerBridgeTest, ConsolidateWakeLocks) {
  AcquireDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));

  // Acquiring a second Android wake lock of the same time shouldn't result in a
  // second device service wake lock being requested.
  AcquireDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));

  ReleaseDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));

  // The device service wake lock should only be released when all Android wake
  // locks have been released.
  ReleaseDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(0, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));
}

TEST_F(ArcPowerBridgeTest, ReleaseWakeLocksWhenInstanceClosed) {
  AcquireDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  ASSERT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));

  // If the instance is closed, all wake locks should be released.
  base::RunLoop run_loop;
  DestroyPowerInstance();
  run_loop.RunUntilIdle();
  EXPECT_EQ(0, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));

  // Check that wake locks can be requested after the instance becomes ready
  // again.
  CreatePowerInstance();
  AcquireDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));
}

}  // namespace arc
