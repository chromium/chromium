// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/unlock_manager_impl.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_simple_task_runner.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/proximity_auth/fake_lock_handler.h"
#include "chromeos/ash/components/proximity_auth/fake_remote_device_life_cycle.h"
#include "chromeos/ash/components/proximity_auth/messenger.h"
#include "chromeos/ash/components/proximity_auth/mock_proximity_auth_client.h"
#include "chromeos/ash/components/proximity_auth/proximity_monitor.h"
#include "chromeos/ash/components/proximity_auth/remote_device_life_cycle.h"
#include "chromeos/ash/components/proximity_auth/remote_status_update.h"
#include "chromeos/ash/services/secure_channel/connection.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace proximity_auth {
namespace {

using SmartLockState = ash::SmartLockState;

// Note that the trust agent state is currently ignored by the UnlockManager
// implementation.
RemoteStatusUpdate kRemoteScreenUnlocked = {
    USER_PRESENT, SECURE_SCREEN_LOCK_ENABLED, TRUST_AGENT_UNSUPPORTED};
RemoteStatusUpdate kRemoteScreenLocked = {
    USER_ABSENT, SECURE_SCREEN_LOCK_ENABLED, TRUST_AGENT_UNSUPPORTED};
RemoteStatusUpdate kRemoteScreenlockDisabled = {
    USER_PRESENT, SECURE_SCREEN_LOCK_DISABLED, TRUST_AGENT_UNSUPPORTED};
RemoteStatusUpdate kRemoteScreenlockStateUnknown = {
    USER_PRESENCE_UNKNOWN, SECURE_SCREEN_LOCK_STATE_UNKNOWN,
    TRUST_AGENT_UNSUPPORTED};

class MockMessenger : public Messenger {
 public:
  MockMessenger() {}

  MockMessenger(const MockMessenger&) = delete;
  MockMessenger& operator=(const MockMessenger&) = delete;

  ~MockMessenger() override {}

  MOCK_METHOD1(AddObserver, void(MessengerObserver* observer));
  MOCK_METHOD1(RemoveObserver, void(MessengerObserver* observer));
  MOCK_METHOD0(DispatchUnlockEvent, void());
  MOCK_METHOD1(RequestDecryption, void(const std::string& challenge));
  MOCK_METHOD0(RequestUnlock, void());
  MOCK_CONST_METHOD0(GetConnection, ash::secure_channel::Connection*());
  MOCK_CONST_METHOD0(GetChannel, ash::secure_channel::ClientChannel*());
};

class MockProximityMonitor : public ProximityMonitor {
 public:
  explicit MockProximityMonitor(base::OnceClosure destroy_callback)
      : destroy_callback_(std::move(destroy_callback)), started_(false) {
    ON_CALL(*this, IsUnlockAllowed()).WillByDefault(Return(true));
  }

  MockProximityMonitor(const MockProximityMonitor&) = delete;
  MockProximityMonitor& operator=(const MockProximityMonitor&) = delete;

  ~MockProximityMonitor() override { std::move(destroy_callback_).Run(); }

  void Start() override { started_ = true; }
  void Stop() override {}
  MOCK_CONST_METHOD0(IsUnlockAllowed, bool());
  MOCK_METHOD0(RecordProximityMetricsOnAuthSuccess, void());

  bool started() { return started_; }

 private:
  base::OnceClosure destroy_callback_;
  bool started_;
};

class TestUnlockManager : public UnlockManagerImpl {
 public:
  explicit TestUnlockManager(ProximityAuthClient* proximity_auth_client)
      : UnlockManagerImpl(proximity_auth_client) {}

  TestUnlockManager(const TestUnlockManager&) = delete;
  TestUnlockManager& operator=(const TestUnlockManager&) = delete;

  ~TestUnlockManager() override {}

  using MessengerObserver::OnDisconnected;
  using MessengerObserver::OnRemoteStatusUpdate;
  using MessengerObserver::OnUnlockEventSent;
  using MessengerObserver::OnUnlockResponse;
  using UnlockManager::OnAuthAttempted;

  MockProximityMonitor* proximity_monitor() { return proximity_monitor_; }
  bool proximity_monitor_destroyed() { return proximity_monitor_destroyed_; }

 private:
  std::unique_ptr<ProximityMonitor> CreateProximityMonitor(
      RemoteDeviceLifeCycle* life_cycle) override {
    std::unique_ptr<MockProximityMonitor> proximity_monitor(
        new NiceMock<MockProximityMonitor>(
            base::BindOnce(&TestUnlockManager::OnProximityMonitorDestroyed,
                           base::Unretained(this))));
    proximity_monitor_destroyed_ = false;

    proximity_monitor_ = proximity_monitor.get();
    return proximity_monitor;
  }

  void OnProximityMonitorDestroyed() { proximity_monitor_destroyed_ = true; }

  // Owned by the super class.
  raw_ptr<MockProximityMonitor, DanglingUntriaged> proximity_monitor_ = nullptr;
  bool proximity_monitor_destroyed_ = false;
};

// Creates a mock Bluetooth adapter and sets it as the global adapter for
// testing.
scoped_refptr<device::MockBluetoothAdapter>
CreateAndRegisterMockBluetoothAdapter() {
  scoped_refptr<device::MockBluetoothAdapter> adapter =
      new NiceMock<device::MockBluetoothAdapter>();
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);
  return adapter;
}

}  // namespace

class ProximityAuthUnlockManagerImplTest : public testing::Test {
 public:
  ProximityAuthUnlockManagerImplTest()
      : remote_device_(ash::multidevice::CreateRemoteDeviceRefForTest()),
        local_device_(ash::multidevice::CreateRemoteDeviceRefForTest()),
        life_cycle_(remote_device_, local_device_),
        fake_client_channel_(
            std::make_unique<ash::secure_channel::FakeClientChannel>()),
        bluetooth_adapter_(CreateAndRegisterMockBluetoothAdapter()),
        task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_current_default_handle_(task_runner_) {}

  ~ProximityAuthUnlockManagerImplTest() override = default;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();

    ON_CALL(*bluetooth_adapter_, IsPresent()).WillByDefault(Return(true));
    ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(true));

    ON_CALL(messenger_, GetChannel())
        .WillByDefault(Return(fake_client_channel_.get()));
    life_cycle_.set_messenger(&messenger_);
    life_cycle_.set_channel(fake_client_channel_.get());
  }

  void TearDown() override {
    // Make sure to verify the mock prior to the destruction of the unlock
    // manager, as otherwise it's impossible to tell whether calls to Stop()
    // occur as a side-effect of the destruction or from the code intended to be
    // under test.
    if (proximity_monitor())
      testing::Mock::VerifyAndClearExpectations(proximity_monitor());

    unlock_manager_.reset();

    chromeos::PowerManagerClient::Shutdown();
  }

  void CreateUnlockManager() {
    unlock_manager_ =
        std::make_unique<TestUnlockManager>(&proximity_auth_client_);

    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_bluetooth_suspension_recovery_timer_ = mock_timer.get();
    unlock_manager_->SetBluetoothSuspensionRecoveryTimerForTesting(
        std::move(mock_timer));
  }

  void SimulateUserPresentState() {
    unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
    life_cycle_.ChangeState(
        RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
    unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);
  }

  void RunPendingTasks() { task_runner_->RunPendingTasks(); }

  MockProximityMonitor* proximity_monitor() {
    return unlock_manager_ ? unlock_manager_->proximity_monitor() : nullptr;
  }

  bool proximity_monitor_destroyed() {
    return unlock_manager_ ? unlock_manager_->proximity_monitor_destroyed()
                           : false;
  }

 protected:
  ash::multidevice::RemoteDeviceRef remote_device_;
  ash::multidevice::RemoteDeviceRef local_device_;
  FakeRemoteDeviceLifeCycle life_cycle_;
  std::unique_ptr<ash::secure_channel::FakeClientChannel> fake_client_channel_;

  // Mock used for verifying interactions with the Bluetooth subsystem.
  scoped_refptr<device::MockBluetoothAdapter> bluetooth_adapter_;

  NiceMock<MockProximityAuthClient> proximity_auth_client_;
  NiceMock<MockMessenger> messenger_;
  std::unique_ptr<TestUnlockManager> unlock_manager_;
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged>
      mock_bluetooth_suspension_recovery_timer_ = nullptr;

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      thread_task_runner_current_default_handle_;
  FakeLockHandler lock_handler_;
  ash::multidevice::ScopedDisableLoggingForTesting disable_logging_;
};

TEST_F(ProximityAuthUnlockManagerImplTest, IsUnlockAllowed_InitialState) {
  CreateUnlockManager();
  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_SessionLock_AllGood) {
  CreateUnlockManager();

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_TRUE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_DisallowedByProximityMonitor) {
  CreateUnlockManager();

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);

  ON_CALL(*proximity_monitor(), IsUnlockAllowed()).WillByDefault(Return(false));
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);
  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_RemoteDeviceLifeCycleIsNull) {
  CreateUnlockManager();

  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_RemoteScreenlockStateLocked) {
  CreateUnlockManager();

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenLocked);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest, IsUnlockAllowed_UserIsSecondary) {
  CreateUnlockManager();

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnRemoteStatusUpdate({USER_PRESENCE_SECONDARY,
                                         SECURE_SCREEN_LOCK_ENABLED,
                                         TRUST_AGENT_UNSUPPORTED});

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_PrimaryUserInBackground) {
  CreateUnlockManager();

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnRemoteStatusUpdate({USER_PRESENCE_BACKGROUND,
                                         SECURE_SCREEN_LOCK_ENABLED,
                                         TRUST_AGENT_UNSUPPORTED});

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_RemoteScreenlockStateUnknown) {
  CreateUnlockManager();

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenlockStateUnknown);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_RemoteScreenlockStateDisabled) {
  CreateUnlockManager();

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenlockDisabled);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_RemoteScreenlockStateNotYetReceived) {
  CreateUnlockManager();

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest, SetRemoteDeviceLifeCycle_SetToNull) {
  CreateUnlockManager();
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kInactive));
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       SetRemoteDeviceLifeCycle_ExistingRemoteDeviceLifeCycle) {
  CreateUnlockManager();
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kConnectingToPhone));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       SetRemoteDeviceLifeCycle_AuthenticationFailed) {
  CreateUnlockManager();
  SimulateUserPresentState();

  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);

  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED);

  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kPhoneNotAuthenticated));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
}

TEST_F(ProximityAuthUnlockManagerImplTest, SetRemoteDeviceLifeCycle_WakingUp) {
  CreateUnlockManager();
  SimulateUserPresentState();

  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);

  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::FINDING_CONNECTION);

  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kConnectingToPhone));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       SetRemoteDeviceLifeCycle_TimesOutBeforeConnection) {
  CreateUnlockManager();

  life_cycle_.set_messenger(nullptr);
  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::FINDING_CONNECTION);

  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kConnectingToPhone));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kPhoneNotFound));
  // Simulate timing out before a connection is established.
  RunPendingTasks();
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       SetRemoteDeviceLifeCycle_NullRemoteDeviceLifeCycle_NoProximityMonitor) {
  CreateUnlockManager();
  SimulateUserPresentState();
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
}

TEST_F(
    ProximityAuthUnlockManagerImplTest,
    SetRemoteDeviceLifeCycle_ConnectingRemoteDeviceLifeCycle_StopsProximityMonitor) {
  CreateUnlockManager();
  SimulateUserPresentState();

  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::FINDING_CONNECTION);
  EXPECT_TRUE(proximity_monitor_destroyed());
}

TEST_F(
    ProximityAuthUnlockManagerImplTest,
    SetRemoteDeviceLifeCycle_ConnectedRemoteDeviceLifeCycle_StartsProximityMonitor) {
  CreateUnlockManager();
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  EXPECT_TRUE(proximity_monitor()->started());
}

// Regression test for crbug.com/931929. Capture the case where the phone is
// connected to, connection is lost, and then a new connection is made shortly
// after.
TEST_F(ProximityAuthUnlockManagerImplTest,
       SetRemoteDeviceLifeCycle_TwiceConnectedRemoteDeviceLifeCycle) {
  CreateUnlockManager();

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  EXPECT_TRUE(proximity_monitor()->started());

  // Simulate the phone connection being lost. The ProximityMonitor is stale
  // and should have been destroyed.
  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::FINDING_CONNECTION);
  EXPECT_TRUE(proximity_monitor_destroyed());

  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  EXPECT_FALSE(proximity_monitor_destroyed());
  EXPECT_TRUE(proximity_monitor()->started());
}

TEST_F(ProximityAuthUnlockManagerImplTest, BluetoothAdapterNotPresent) {
  ON_CALL(*bluetooth_adapter_, IsPresent()).WillByDefault(Return(false));

  CreateUnlockManager();

  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kBluetoothDisabled));

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  EXPECT_FALSE(life_cycle_.started());
}

TEST_F(ProximityAuthUnlockManagerImplTest, BluetoothAdapterPowerChanges) {
  ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(false));

  CreateUnlockManager();

  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kBluetoothDisabled));

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  EXPECT_FALSE(life_cycle_.started());

  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kConnectingToPhone));
  ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(true));
  bluetooth_adapter_->NotifyAdapterPoweredChanged(true);
  EXPECT_TRUE(life_cycle_.started());
}

TEST_F(
    ProximityAuthUnlockManagerImplTest,
    CacheBluetoothAdapterStateAfterSuspendAndResume_AttemptConnectionWhileBluetoothAdapterIsStillRecovering) {
  CreateUnlockManager();

  ASSERT_FALSE(mock_bluetooth_suspension_recovery_timer_->IsRunning());

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_LID_CLOSED);

  // Simulate https://crbug.com/986896 by returning false for presence and power
  // directly after resuming, but do not fire
  // |mock_bluetooth_suspension_recovery_timer_|, simulating that not enough
  // time has passed for the BluetoothAdapter to recover. It's expected under
  // these conditions that:
  // * ProximityAuthClient::UpdateSmartLockState() never be called with
  //   SmartLockState::kBluetoothDisabled.
  // * ProximityAuthClient::UpdateSmartLockState() only be called once with
  //   SmartLockState::BLUETOOTH_CONNECTING, because it should only be called
  //   on when the SmartLockState value changes.
  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kBluetoothDisabled))
      .Times(0);
  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kConnectingToPhone));

  ON_CALL(*bluetooth_adapter_, IsPresent()).WillByDefault(Return(false));
  ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(false));

  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  EXPECT_TRUE(mock_bluetooth_suspension_recovery_timer_->IsRunning());

  // Simulate how ProximityAuthSystem, the owner of UnlockManager, reacts to
  // resume: providing a new RemoteDeviceLifeCycle. This shouldn't trigger a new
  // call to ProximityAuthClient::UpdateSmartLockState().
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  EXPECT_TRUE(life_cycle_.started());

  EXPECT_TRUE(mock_bluetooth_suspension_recovery_timer_->IsRunning());
}

TEST_F(
    ProximityAuthUnlockManagerImplTest,
    CacheBluetoothAdapterStateAfterSuspendAndResume_AttemptConnectionOnceBluetoothAdapterHasHadTimeToRecover) {
  CreateUnlockManager();

  ASSERT_FALSE(mock_bluetooth_suspension_recovery_timer_->IsRunning());

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_LID_CLOSED);

  // Simulate https://crbug.com/986896 by returning false for presence and power
  // directly after resuming, and then fire
  // |mock_bluetooth_suspension_recovery_timer_|, simulating that enough time
  // has passed for the BluetoothAdapter to recover - this means that Bluetooth
  // is truly off after resume and the user should be visually informed as such.
  // It's expected under these conditions that:
  // * ProximityAuthClient::UpdateSmartLockState() only be called once with
  //   SmartLockState::kBluetoothDisabled, but after the timer fires (this is
  //   impossible to explicitly do in code with mocks, unfortunately).
  // * ProximityAuthClient::UpdateSmartLockState() only be called once with
  //   SmartLockState::BLUETOOTH_CONNECTING, directly after SuspendDone.
  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kBluetoothDisabled));
  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kConnectingToPhone));

  ON_CALL(*bluetooth_adapter_, IsPresent()).WillByDefault(Return(false));
  ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(false));

  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  EXPECT_TRUE(mock_bluetooth_suspension_recovery_timer_->IsRunning());

  // Simulate how ProximityAuthSystem, the owner of UnlockManager, reacts to
  // resume: providing a new RemoteDeviceLifeCycle. This shouldn't trigger a new
  // call to ProximityAuthClient::UpdateSmartLockState().
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  EXPECT_TRUE(life_cycle_.started());

  ON_CALL(*bluetooth_adapter_, IsPresent()).WillByDefault(Return(false));
  ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(false));

  EXPECT_TRUE(mock_bluetooth_suspension_recovery_timer_->IsRunning());

  // This leads to ProximityAuthClient::UpdateSmartLockState() being called
  // with SmartLockState::NO_BLUETOOTH.
  mock_bluetooth_suspension_recovery_timer_->Fire();
}

TEST_F(
    ProximityAuthUnlockManagerImplTest,
    InitialScanAfterSuspendResume_DontPerformInitialScanIfConnectionEstablished) {
  CreateUnlockManager();

  ASSERT_FALSE(mock_bluetooth_suspension_recovery_timer_->IsRunning());

  // Simulates the lid of chromebook closing resulting in suspension.
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_LID_CLOSED);

  EXPECT_CALL(
      proximity_auth_client_,
      UpdateSmartLockState(SmartLockState::kPhoneFoundLockedAndProximate))
      .Times(1);
  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kConnectingToPhone))
      .Times(1);

  // We want to emulate a bluetooth adapter that is present and powered
  // upon lid reopen and resume, because we are testing the code path
  // within OnBluetoothAdapterPresentAndPowerChanged() after the
  // suspension timer concludes.
  ON_CALL(*bluetooth_adapter_, IsPresent()).WillByDefault(Return(true));
  ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(true));

  // This event simulates reopen of lid resulting in resume.
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  EXPECT_TRUE(mock_bluetooth_suspension_recovery_timer_->IsRunning());

  // Start the life cycle for unlock manager.
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  EXPECT_TRUE(life_cycle_.started());

  // Simulate a secure channel connection established with phone.
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);

  // Simulate the phone responding with locked and proximate.
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenLocked);

  EXPECT_TRUE(mock_bluetooth_suspension_recovery_timer_->IsRunning());

  // Time out the suspension recovery timer so we run
  // OnBluetoothAdapterPresentAndPowerChanged().
  mock_bluetooth_suspension_recovery_timer_->Fire();
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       BluetoothOffMessageShownImmediatelyIfBluetoothWasOffBeforeSuspend) {
  CreateUnlockManager();

  ON_CALL(*bluetooth_adapter_, IsPresent()).WillByDefault(Return(false));
  ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(false));

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_LID_CLOSED);

  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kBluetoothDisabled));
  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kConnectingToPhone))
      .Times(0);

  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();

  // Simulate how ProximityAuthSystem, the owner of UnlockManager, reacts to
  // resume: providing a new RemoteDeviceLifeCycle.
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  EXPECT_FALSE(life_cycle_.started());
}

TEST_F(ProximityAuthUnlockManagerImplTest, StartsProximityMonitor) {
  CreateUnlockManager();
  SimulateUserPresentState();
  EXPECT_TRUE(proximity_monitor()->started());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthenticationFailed_StopsProximityMonitor) {
  CreateUnlockManager();
  SimulateUserPresentState();

  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED);
  EXPECT_TRUE(proximity_monitor_destroyed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       AuthenticationFailed_UpdatesSmartLockState) {
  CreateUnlockManager();
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kPhoneNotAuthenticated));
  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       FindingConnection_UpdatesSmartLockState) {
  CreateUnlockManager();

  // Regression test for https://crbug.com/890047, ensuring that the NO_PHONE
  // status doesn't incorrectly appear for a brief moment before the
  // BLUETOOTH_CONNECTING spinner.
  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kPhoneNotFound))
      .Times(0);

  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kConnectingToPhone));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  EXPECT_TRUE(life_cycle_.started());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       Authenticating_UpdatesSmartLockState) {
  CreateUnlockManager();

  // Regression test for https://crbug.com/890047, ensuring that the NO_PHONE
  // status doesn't incorrectly appear for a brief moment before the
  // BLUETOOTH_CONNECTING spinner.
  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kPhoneNotFound))
      .Times(0);

  EXPECT_CALL(proximity_auth_client_,
              UpdateSmartLockState(SmartLockState::kConnectingToPhone));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  EXPECT_TRUE(life_cycle_.started());
  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::AUTHENTICATING);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnDecryptResponse_NoAuthAttemptInProgress) {
  CreateUnlockManager();
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnUnlockEventSent_NoAuthAttemptInProgress) {
  CreateUnlockManager();
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_.get()->OnUnlockEventSent(true);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnUnlockResponse_NoAuthAttemptInProgress) {
  CreateUnlockManager();
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_.get()->OnUnlockResponse(true);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_NoRemoteDeviceLifeCycle) {
  CreateUnlockManager();
  SimulateUserPresentState();

  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerImplTest, OnAuthAttempted_UnlockNotAllowed) {
  CreateUnlockManager();
  SimulateUserPresentState();

  ON_CALL(*proximity_monitor(), IsUnlockAllowed()).WillByDefault(Return(false));

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerImplTest, OnAuthAttempted_NotUserClick) {
  CreateUnlockManager();
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_->OnAuthAttempted(mojom::AuthType::EXPAND_THEN_USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerImplTest, OnAuthAttempted_DuplicateCall) {
  CreateUnlockManager();
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(messenger_, RequestUnlock()).Times(0);
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerImplTest, OnAuthAttempted_TimesOut) {
  CreateUnlockManager();
  SimulateUserPresentState();

  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  // Simulate the timeout period elapsing.
  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  RunPendingTasks();
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_DoesntTimeOutFollowingResponse) {
  CreateUnlockManager();
  SimulateUserPresentState();

  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_));
  unlock_manager_->OnUnlockResponse(false);

  // Simulate the timeout period elapsing.
  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  RunPendingTasks();
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_Unlock_UnlockRequestFails) {
  CreateUnlockManager();
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnUnlockResponse(false);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_Unlock_WithSignIn_RequestSucceeds_EventSendFails) {
  CreateUnlockManager();
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(messenger_, DispatchUnlockEvent());
  unlock_manager_->OnUnlockResponse(true);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnUnlockEventSent(false);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_Unlock_RequestSucceeds_EventSendSucceeds) {
  CreateUnlockManager();
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(messenger_, DispatchUnlockEvent());
  unlock_manager_->OnUnlockResponse(true);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(true));
  unlock_manager_->OnUnlockEventSent(true);
}

}  // namespace proximity_auth
