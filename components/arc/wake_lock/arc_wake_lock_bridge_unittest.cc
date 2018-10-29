// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/wake_lock/arc_wake_lock_bridge.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/power.mojom.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_power_instance.h"
#include "components/arc/test/fake_wake_lock_instance.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

using device::mojom::WakeLockType;

class ArcWakeLockBridgeTest : public testing::Test {
 public:
  ArcWakeLockBridgeTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME) {
    auto wake_lock_provider_ptr =
        std::make_unique<device::TestWakeLockProvider>();
    wake_lock_provider_ = wake_lock_provider_ptr.get();

    connector_factory_ =
        service_manager::TestConnectorFactory::CreateForUniqueService(
            std::move(wake_lock_provider_ptr));
    connector_ = connector_factory_->CreateConnector();

    fake_power_manager_client_ = new chromeos::FakePowerManagerClient;
    chromeos::DBusThreadManager::GetSetterForTesting()->SetPowerManagerClient(
        base::WrapUnique(fake_power_manager_client_));

    bridge_service_ = std::make_unique<ArcBridgeService>();
    wake_lock_bridge_ =
        std::make_unique<ArcWakeLockBridge>(nullptr, bridge_service_.get());
    wake_lock_bridge_->set_connector_for_testing(connector_.get());
    CreateWakeLockInstance();
  }

  ~ArcWakeLockBridgeTest() override { DestroyWakeLockInstance(); }

 protected:
  // Creates a FakeWakeLockInstance for |bridge_service_|. This results in
  // ArcWakeLockBridge::OnInstanceReady() being called.
  void CreateWakeLockInstance() {
    instance_ = std::make_unique<FakeWakeLockInstance>();
    bridge_service_->wake_lock()->SetInstance(instance_.get());
    WaitForInstanceReady(bridge_service_->wake_lock());
  }

  // Destroys the FakeWakeLockInstance. This results in
  // ArcWakeLockBridge::OnInstanceClosed() being called.
  void DestroyWakeLockInstance() {
    if (!instance_)
      return;
    bridge_service_->wake_lock()->CloseInstance(instance_.get());
    instance_.reset();
  }

  device::TestWakeLockProvider* GetWakeLockProvider() const {
    return wake_lock_provider_;
  }

  // Returns true iff there is no failure acquiring a system wake lock.
  bool AcquirePartialWakeLock() {
    base::RunLoop loop;
    bool result = false;
    wake_lock_bridge_->AcquirePartialWakeLock(base::BindOnce(
        [](bool* result_out, bool result) { *result_out = result; }, &result));
    loop.RunUntilIdle();
    wake_lock_bridge_->FlushWakeLocksForTesting();
    return result;
  }

  // Returns true iff there is no failure releasing a system wake lock.
  bool ReleasePartialWakeLock() {
    base::RunLoop loop;
    bool result = false;
    wake_lock_bridge_->ReleasePartialWakeLock(base::BindOnce(
        [](bool* result_out, bool result) { *result_out = result; }, &result));
    loop.RunUntilIdle();
    wake_lock_bridge_->FlushWakeLocksForTesting();
    return result;
  }

  // Return true iff all dark resume related state is set i.e the suspend
  // readiness callback is set and wake lock release event has observers.
  bool IsDarkResumeStateSet() const {
    return wake_lock_bridge_->IsSuspendReadinessStateSetForTesting() &&
           wake_lock_bridge_->WakeLockHasObserversForTesting(
               WakeLockType::kPreventAppSuspension);
  }

  // Return true iff all dark resume related state is reset. This should be true
  // when device exits dark resume either by re-suspending or transitioning to
  // full resume.
  bool IsDarkResumeStateReset() const {
    return !wake_lock_bridge_->WakeLockHasObserversForTesting(
               WakeLockType::kPreventAppSuspension) &&
           !wake_lock_bridge_->IsSuspendReadinessStateSetForTesting();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // Owned by chromeos::DBusThreadManager.
  chromeos::FakePowerManagerClient* fake_power_manager_client_;

 private:
  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;
  device::TestWakeLockProvider* wake_lock_provider_;

  std::unique_ptr<ArcBridgeService> bridge_service_;
  std::unique_ptr<FakeWakeLockInstance> instance_;
  std::unique_ptr<ArcWakeLockBridge> wake_lock_bridge_;

  DISALLOW_COPY_AND_ASSIGN(ArcWakeLockBridgeTest);
};

TEST_F(ArcWakeLockBridgeTest, AcquireAndReleaseSinglePartialWakeLock) {
  EXPECT_TRUE(AcquirePartialWakeLock());
  EXPECT_EQ(1, GetWakeLockProvider()->GetActiveWakeLocksOfType(
                   WakeLockType::kPreventAppSuspension));

  EXPECT_TRUE(ReleasePartialWakeLock());
  EXPECT_EQ(0, GetWakeLockProvider()->GetActiveWakeLocksOfType(
                   WakeLockType::kPreventAppSuspension));
}

TEST_F(ArcWakeLockBridgeTest, AcquireAndReleaseMultiplePartialWakeLocks) {
  // Taking multiple wake locks should result in only one active wake lock.
  EXPECT_TRUE(AcquirePartialWakeLock());
  EXPECT_TRUE(AcquirePartialWakeLock());
  EXPECT_TRUE(AcquirePartialWakeLock());
  EXPECT_EQ(1, GetWakeLockProvider()->GetActiveWakeLocksOfType(
                   WakeLockType::kPreventAppSuspension));

  // Releasing two wake locks after acquiring three should not result in
  // releasing a wake lock.
  EXPECT_TRUE(ReleasePartialWakeLock());
  EXPECT_TRUE(ReleasePartialWakeLock());
  EXPECT_EQ(1, GetWakeLockProvider()->GetActiveWakeLocksOfType(
                   WakeLockType::kPreventAppSuspension));

  // Releasing the remaining wake lock should result in the release of the wake
  // lock.
  EXPECT_TRUE(ReleasePartialWakeLock());
  EXPECT_EQ(0, GetWakeLockProvider()->GetActiveWakeLocksOfType(
                   WakeLockType::kPreventAppSuspension));
}

TEST_F(ArcWakeLockBridgeTest, ReleaseWakeLockOnInstanceClosed) {
  EXPECT_TRUE(AcquirePartialWakeLock());
  ASSERT_EQ(1, GetWakeLockProvider()->GetActiveWakeLocksOfType(
                   WakeLockType::kPreventAppSuspension));

  // If the instance is closed, all wake locks should be released.
  base::RunLoop run_loop;
  GetWakeLockProvider()->set_wake_lock_canceled_callback(
      run_loop.QuitClosure());
  DestroyWakeLockInstance();
  run_loop.Run();
  EXPECT_EQ(0, GetWakeLockProvider()->GetActiveWakeLocksOfType(
                   WakeLockType::kPreventDisplaySleep));

  // Check that wake locks can be requested after the instance becomes ready
  // again.
  CreateWakeLockInstance();
  EXPECT_TRUE(AcquirePartialWakeLock());
  EXPECT_EQ(1, GetWakeLockProvider()->GetActiveWakeLocksOfType(
                   WakeLockType::kPreventAppSuspension));
}

TEST_F(ArcWakeLockBridgeTest, CheckSuspendAfterDarkResumeNoWakeLocksHeld) {
  // Trigger a dark resume event, move time forward to trigger a wake lock check
  // and check if a re-suspend happened if no wake locks were acquired.
  fake_power_manager_client_->SendDarkSuspendImminent();
  scoped_task_environment_.FastForwardBy(
      ArcWakeLockBridge::kDarkResumeWakeLockCheckTimeout);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_TRUE(IsDarkResumeStateReset());

  // Trigger a dark resume event, acquire and release a wake lock and move time
  // forward to trigger a wake lock check. The device should re-suspend in this
  // case since no wake locks were held at the time of the wake lock check.
  fake_power_manager_client_->SendDarkSuspendImminent();
  EXPECT_TRUE(AcquirePartialWakeLock());
  EXPECT_TRUE(ReleasePartialWakeLock());
  scoped_task_environment_.FastForwardBy(
      ArcWakeLockBridge::kDarkResumeWakeLockCheckTimeout);
  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();
  EXPECT_TRUE(IsDarkResumeStateReset());
}

TEST_F(ArcWakeLockBridgeTest, CheckSuspendAfterDarkResumeWakeLocksHeld) {
  // Trigger a dark resume event, acquire a wake lock and move time forward to a
  // wake lock check. At this point the system shouldn't re-suspend i.e. the
  // suspend readiness callback should be set and wake lock release should have
  // observers.
  fake_power_manager_client_->SendDarkSuspendImminent();
  EXPECT_TRUE(AcquirePartialWakeLock());
  scoped_task_environment_.FastForwardBy(
      ArcWakeLockBridge::kDarkResumeWakeLockCheckTimeout);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_TRUE(IsDarkResumeStateSet());

  // Move time forward by < |kDarkResumeHardTimeout| and release the
  // partial wake lock.This should instantaneously re-suspend the device.
  scoped_task_environment_.FastForwardBy(
      ArcWakeLockBridge::kDarkResumeHardTimeout -
      base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(ReleasePartialWakeLock());
  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();
  EXPECT_TRUE(IsDarkResumeStateReset());
}

TEST_F(ArcWakeLockBridgeTest, CheckSuspendAfterDarkResumeHardTimeout) {
  // Trigger a dark resume event, acquire a wake lock and move time forward to a
  // wake lock check. At this point the system shouldn't re-suspend i.e. the
  // suspend readiness callback should be set and wake lock release should have
  // observers.
  fake_power_manager_client_->SendDarkSuspendImminent();
  EXPECT_TRUE(AcquirePartialWakeLock());
  scoped_task_environment_.FastForwardBy(
      ArcWakeLockBridge::kDarkResumeWakeLockCheckTimeout);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_TRUE(IsDarkResumeStateSet());

  // Move time forward by |kDarkResumeHardTimeout|. At this point the
  // device should re-suspend even though the wake lock is acquired.
  scoped_task_environment_.FastForwardBy(
      ArcWakeLockBridge::kDarkResumeHardTimeout);
  EXPECT_EQ(1, GetWakeLockProvider()->GetActiveWakeLocksOfType(
                   WakeLockType::kPreventAppSuspension));
  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();
  EXPECT_TRUE(IsDarkResumeStateReset());
}

TEST_F(ArcWakeLockBridgeTest, CheckStateResetAfterSuspendDone) {
  // Trigger a dark resume event, acquire a wake lock and move time forward to a
  // wake lock check. At this point the system shouldn't re-suspend i.e. the
  // suspend readiness callback should be set and wake lock release should have
  // observers.
  fake_power_manager_client_->SendDarkSuspendImminent();
  EXPECT_TRUE(AcquirePartialWakeLock());
  scoped_task_environment_.FastForwardBy(
      ArcWakeLockBridge::kDarkResumeWakeLockCheckTimeout);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_TRUE(IsDarkResumeStateSet());

  // Trigger suspend done event. Check if state is reset as dark resume would be
  // exited.
  fake_power_manager_client_->SendSuspendDone();
  EXPECT_TRUE(IsDarkResumeStateReset());
}

}  // namespace arc
