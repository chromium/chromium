// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/unlock_manager_impl.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/components/proximity_auth/fake_lock_handler.h"
#include "chromeos/components/proximity_auth/fake_remote_device_life_cycle.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/components/proximity_auth/messenger.h"
#include "chromeos/components/proximity_auth/mock_proximity_auth_client.h"
#include "chromeos/components/proximity_auth/proximity_monitor.h"
#include "chromeos/components/proximity_auth/remote_device_life_cycle.h"
#include "chromeos/components/proximity_auth/remote_status_update.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "components/cryptauth/fake_connection.h"
#include "components/cryptauth/fake_secure_context.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/cryptauth/secure_context.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AtLeast;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::_;

namespace proximity_auth {
namespace {

// The sign-in challenge to send to the remote device.
const char kChallenge[] = "sign-in challenge";
const char kSignInSecret[] = "decrypted challenge";

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
  ~MockMessenger() override {}

  MOCK_METHOD1(AddObserver, void(MessengerObserver* observer));
  MOCK_METHOD1(RemoveObserver, void(MessengerObserver* observer));
  MOCK_CONST_METHOD0(SupportsSignIn, bool());
  MOCK_METHOD0(DispatchUnlockEvent, void());
  MOCK_METHOD1(RequestDecryption, void(const std::string& challenge));
  MOCK_METHOD0(RequestUnlock, void());
  MOCK_CONST_METHOD0(GetSecureContext, cryptauth::SecureContext*());
  MOCK_CONST_METHOD0(GetConnection, cryptauth::Connection*());
  MOCK_CONST_METHOD0(GetChannel, chromeos::secure_channel::ClientChannel*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMessenger);
};

class MockProximityMonitor : public ProximityMonitor {
 public:
  MockProximityMonitor() : started_(false), stopped_(false) {
    ON_CALL(*this, IsUnlockAllowed()).WillByDefault(Return(true));
  }
  ~MockProximityMonitor() override {}

  void Start() override { started_ = true; }
  void Stop() override { stopped_ = true; }
  MOCK_CONST_METHOD0(IsUnlockAllowed, bool());
  MOCK_METHOD0(RecordProximityMetricsOnAuthSuccess, void());
  MOCK_METHOD1(AddObserver, void(ProximityMonitorObserver*));
  MOCK_METHOD1(RemoveObserver, void(ProximityMonitorObserver*));

  bool started() { return started_; }
  bool stopped() { return stopped_; }

 private:
  bool started_;
  bool stopped_;

  DISALLOW_COPY_AND_ASSIGN(MockProximityMonitor);
};

class TestUnlockManager : public UnlockManagerImpl {
 public:
  TestUnlockManager(ProximityAuthSystem::ScreenlockType screenlock_type,
                    ProximityAuthClient* proximity_auth_client)
      : UnlockManagerImpl(screenlock_type, proximity_auth_client, nullptr),
        proximity_monitor_(nullptr) {}
  ~TestUnlockManager() override {}

  using UnlockManager::OnAuthAttempted;
  using MessengerObserver::OnUnlockEventSent;
  using MessengerObserver::OnRemoteStatusUpdate;
  using MessengerObserver::OnDecryptResponse;
  using MessengerObserver::OnUnlockResponse;
  using MessengerObserver::OnDisconnected;
  using ScreenlockBridge::Observer::OnScreenDidLock;
  using ScreenlockBridge::Observer::OnScreenDidUnlock;
  using ScreenlockBridge::Observer::OnFocusedUserChanged;

  MockProximityMonitor* proximity_monitor() { return proximity_monitor_; }

 private:
  std::unique_ptr<ProximityMonitor> CreateProximityMonitor(
      RemoteDeviceLifeCycle* life_cycle,
      ProximityAuthPrefManager* pref_manager) override {
    EXPECT_EQ(cryptauth::kTestRemoteDevicePublicKey,
              life_cycle->GetConnection()->remote_device().public_key());
    std::unique_ptr<MockProximityMonitor> proximity_monitor(
        new NiceMock<MockProximityMonitor>());
    proximity_monitor_ = proximity_monitor.get();
    return std::move(proximity_monitor);
  }

  // Owned by the super class.
  MockProximityMonitor* proximity_monitor_;

  DISALLOW_COPY_AND_ASSIGN(TestUnlockManager);
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
      : remote_device_(cryptauth::CreateRemoteDeviceRefForTest()),
        local_device_(cryptauth::CreateRemoteDeviceRefForTest()),
        life_cycle_(remote_device_, local_device_),
        connection_(remote_device_),
        fake_client_channel_(
            std::make_unique<chromeos::secure_channel::FakeClientChannel>()),
        bluetooth_adapter_(CreateAndRegisterMockBluetoothAdapter()),
        task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_handle_(task_runner_) {}

  ~ProximityAuthUnlockManagerImplTest() override {
    // Make sure to verify the mock prior to the destruction of the unlock
    // manager, as otherwise it's impossible to tell whether calls to Stop()
    // occur as a side-effect of the destruction or from the code intended to be
    // under test.
    if (proximity_monitor())
      testing::Mock::VerifyAndClearExpectations(proximity_monitor());

    // The UnlockManager must be destroyed before calling
    // chromeos::DBusThreadManager::Shutdown(), as the UnlockManager's
    // destructor references the DBusThreadManager.
    unlock_manager_.reset();

    chromeos::DBusThreadManager::Shutdown();

    ScreenlockBridge::Get()->SetLockHandler(nullptr);
  }

  void InitializeTest(bool multidevice_flags_enabled) {
    SetMultiDeviceApi(multidevice_flags_enabled);

    ON_CALL(*bluetooth_adapter_, IsPresent()).WillByDefault(Return(true));
    ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(true));
    ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(true));
    ON_CALL(messenger_, GetSecureContext())
        .WillByDefault(Return(&secure_context_));
    ON_CALL(messenger_, GetChannel())
        .WillByDefault(Return(fake_client_channel_.get()));

    life_cycle_.set_connection(&connection_);
    life_cycle_.set_messenger(&messenger_);
    life_cycle_.set_channel(fake_client_channel_.get());
    ScreenlockBridge::Get()->SetLockHandler(&lock_handler_);

    chromeos::DBusThreadManager::Initialize();
  }

  void SetMultiDeviceApi(bool enabled) {
    static const std::vector<base::Feature> kFeatures{
        chromeos::features::kMultiDeviceApi,
        chromeos::features::kEnableUnifiedMultiDeviceSetup};

    scoped_feature_list_.InitWithFeatures(
        (enabled ? kFeatures
                 : std::vector<base::Feature>() /* enable_features */),
        (enabled ? std::vector<base::Feature>()
                 : kFeatures /* disable_features */));
  }

  void CreateUnlockManager(
      ProximityAuthSystem::ScreenlockType screenlock_type) {
    unlock_manager_.reset(
        new TestUnlockManager(screenlock_type, &proximity_auth_client_));
  }

  void SimulateUserPresentState() {
    unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
    life_cycle_.ChangeState(
        RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
    unlock_manager_->OnLifeCycleStateChanged();
    unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);
  }

  void RunPendingTasks() { task_runner_->RunPendingTasks(); }

  MockProximityMonitor* proximity_monitor() {
    return unlock_manager_ ? unlock_manager_->proximity_monitor() : nullptr;
  }

 protected:
  cryptauth::RemoteDeviceRef remote_device_;
  cryptauth::RemoteDeviceRef local_device_;
  FakeRemoteDeviceLifeCycle life_cycle_;
  cryptauth::FakeConnection connection_;
  std::unique_ptr<chromeos::secure_channel::FakeClientChannel>
      fake_client_channel_;

  // Mock used for verifying interactions with the Bluetooth subsystem.
  scoped_refptr<device::MockBluetoothAdapter> bluetooth_adapter_;

  NiceMock<MockProximityAuthClient> proximity_auth_client_;
  NiceMock<MockMessenger> messenger_;
  std::unique_ptr<TestUnlockManager> unlock_manager_;
  cryptauth::FakeSecureContext secure_context_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;
  FakeLockHandler lock_handler_;
  ScopedDisableLoggingForTesting disable_logging_;
};

TEST_F(ProximityAuthUnlockManagerImplTest, IsUnlockAllowed_InitialState) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_SessionLock_AllGood) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnLifeCycleStateChanged();
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_TRUE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest, IsUnlockAllowed_SignIn_AllGood) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SIGN_IN);
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnLifeCycleStateChanged();

  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(true));
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_TRUE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_SignIn_MessengerDoesNotSupportSignIn) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SIGN_IN);
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnLifeCycleStateChanged();

  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(false));
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_DisallowedByProximityMonitor) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnLifeCycleStateChanged();

  ON_CALL(*proximity_monitor(), IsUnlockAllowed()).WillByDefault(Return(false));
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);
  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_SecureChannelNotEstablished) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  life_cycle_.set_connection(nullptr);
  life_cycle_.set_messenger(nullptr);
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::AUTHENTICATING);
  unlock_manager_->OnLifeCycleStateChanged();
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_RemoteDeviceLifeCycleIsNull) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_RemoteScreenlockStateLocked) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnLifeCycleStateChanged();
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenLocked);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_RemoteScreenlockStateUnknown) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnLifeCycleStateChanged();
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenlockStateUnknown);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_RemoteScreenlockStateDisabled) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnLifeCycleStateChanged();
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenlockDisabled);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_RemoteScreenlockStateNotYetReceived) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnLifeCycleStateChanged();

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest, SetRemoteDeviceLifeCycle_SetToNull) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::INACTIVE));
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       SetRemoteDeviceLifeCycle_ExistingRemoteDeviceLifeCycle) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, UpdateScreenlockState(_)).Times(0);
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       SetRemoteDeviceLifeCycle_AuthenticationFailed) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);

  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::PHONE_NOT_AUTHENTICATED));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
}

TEST_F(ProximityAuthUnlockManagerImplTest, SetRemoteDeviceLifeCycle_WakingUp) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);

  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::FINDING_CONNECTION);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       SetRemoteDeviceLifeCycle_TimesOutBeforeConnection) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  life_cycle_.set_connection(nullptr);
  life_cycle_.set_messenger(nullptr);
  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::FINDING_CONNECTION);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::NO_PHONE));
  // Simulate timing out before a connection is established.
  RunPendingTasks();
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       SetRemoteDeviceLifeCycle_NullRemoteDeviceLifeCycle_NoProximityMonitor) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
}

TEST_F(
    ProximityAuthUnlockManagerImplTest,
    SetRemoteDeviceLifeCycle_ConnectingRemoteDeviceLifeCycle_StopsProximityMonitor) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->OnLifeCycleStateChanged();
  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::FINDING_CONNECTION);
  unlock_manager_->OnLifeCycleStateChanged();
  EXPECT_TRUE(proximity_monitor()->stopped());
}

TEST_F(
    ProximityAuthUnlockManagerImplTest,
    SetRemoteDeviceLifeCycle_ConnectedRemoteDeviceLifeCycle_StartsProximityMonitor) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  unlock_manager_->OnLifeCycleStateChanged();
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnLifeCycleStateChanged();
  EXPECT_TRUE(proximity_monitor()->started());
}

TEST_F(ProximityAuthUnlockManagerImplTest, BluetoothAdapterNotPresent) {
  InitializeTest(false /* multidevice_flags_enabled */);
  ON_CALL(*bluetooth_adapter_, IsPresent()).WillByDefault(Return(false));

  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::NO_BLUETOOTH));

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  EXPECT_FALSE(life_cycle_.started());
}

TEST_F(ProximityAuthUnlockManagerImplTest, BluetoothAdapterPowerChanges) {
  InitializeTest(false /* multidevice_flags_enabled */);
  ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(false));

  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::NO_BLUETOOTH));

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  EXPECT_FALSE(life_cycle_.started());

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(true));
  bluetooth_adapter_->NotifyAdapterPoweredChanged(true);
  EXPECT_TRUE(life_cycle_.started());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnLifeCycleStateChanged_SecureChannelEstablished_RegistersAsObserver) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();
  EXPECT_CALL(messenger_, AddObserver(unlock_manager_.get()));
  unlock_manager_->OnLifeCycleStateChanged();
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnLifeCycleStateChanged_StartsProximityMonitor) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();
  EXPECT_TRUE(proximity_monitor()->started());
  unlock_manager_->OnLifeCycleStateChanged();
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnLifeCycleStateChanged_StopsProximityMonitor) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->OnLifeCycleStateChanged();
  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED);
  unlock_manager_->OnLifeCycleStateChanged();
  EXPECT_TRUE(proximity_monitor()->stopped());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnLifeCycleStateChanged_AuthenticationFailed_UpdatesScreenlockState) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::PHONE_NOT_AUTHENTICATED));
  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED);
  unlock_manager_->OnLifeCycleStateChanged();
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnLifeCycleStateChanged_FindingConnection_UpdatesScreenlockState) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnLifeCycleStateChanged_Authenticating_UpdatesScreenlockState) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  EXPECT_TRUE(life_cycle_.started());

  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::AUTHENTICATING);
  unlock_manager_->OnLifeCycleStateChanged();
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnDisconnected_UnregistersAsObserver) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();
  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED);
  unlock_manager_->OnLifeCycleStateChanged();

  EXPECT_CALL(messenger_, RemoveObserver(unlock_manager_.get()))
      .Times(testing::AtLeast(1));
  unlock_manager_.get()->OnDisconnected();
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnScreenDidUnlock_StopsProximityMonitor) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_.get()->OnScreenDidUnlock(
      ScreenlockBridge::LockHandler::LOCK_SCREEN);
  EXPECT_TRUE(proximity_monitor()->stopped());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnScreenDidLock_StartsProximityMonitor) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  unlock_manager_.get()->OnScreenDidLock(
      ScreenlockBridge::LockHandler::LOCK_SCREEN);

  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnLifeCycleStateChanged();
  EXPECT_TRUE(proximity_monitor()->started());
}

TEST_F(ProximityAuthUnlockManagerImplTest, OnScreenDidLock_SetsWakingUpState) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));

  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::FINDING_CONNECTION);
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  unlock_manager_.get()->OnScreenDidLock(
      ScreenlockBridge::LockHandler::LOCK_SCREEN);

  unlock_manager_->OnLifeCycleStateChanged();
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnDecryptResponse_NoAuthAttemptInProgress) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_.get()->OnDecryptResponse(kSignInSecret);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnUnlockEventSent_NoAuthAttemptInProgress) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_.get()->OnUnlockEventSent(true);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnUnlockResponse_NoAuthAttemptInProgress) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_.get()->OnUnlockResponse(true);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_NoRemoteDeviceLifeCycle) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerImplTest, OnAuthAttempted_UnlockNotAllowed) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  ON_CALL(*proximity_monitor(), IsUnlockAllowed()).WillByDefault(Return(false));

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerImplTest, OnAuthAttempted_NotUserClick) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_->OnAuthAttempted(mojom::AuthType::EXPAND_THEN_USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerImplTest, OnAuthAttempted_DuplicateCall) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(messenger_, RequestUnlock()).Times(0);
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerImplTest, OnAuthAttempted_TimesOut) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  // Simulate the timeout period elapsing.
  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  RunPendingTasks();
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_DoesntTimeOutFollowingResponse) {
  InitializeTest(false /* multidevice_flags_enabled */);
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_));
  unlock_manager_->OnUnlockResponse(false);

  // Simulate the timeout period elapsing.
  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  RunPendingTasks();
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_Unlock_SupportsSignIn_UnlockRequestFails) {
  InitializeTest(false /* multidevice_flags_enabled */);
  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(true));
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnUnlockResponse(false);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_Unlock_WithSignIn_RequestSucceeds_EventSendFails) {
  InitializeTest(false /* multidevice_flags_enabled */);
  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(true));
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(messenger_, DispatchUnlockEvent());
  unlock_manager_->OnUnlockResponse(true);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnUnlockEventSent(false);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_Unlock_WithSignIn_RequestSucceeds_EventSendSucceeds) {
  InitializeTest(false /* multidevice_flags_enabled */);
  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(true));
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(messenger_, DispatchUnlockEvent());
  unlock_manager_->OnUnlockResponse(true);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(true));
  unlock_manager_->OnUnlockEventSent(true);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_Unlock_DoesntSupportSignIn_UnlockEventSendFails) {
  InitializeTest(false /* multidevice_flags_enabled */);
  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(false));
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, DispatchUnlockEvent());
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnUnlockEventSent(false);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_Unlock_SupportsSignIn_UnlockEventSendSucceeds) {
  InitializeTest(false /* multidevice_flags_enabled */);
  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(false));
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, DispatchUnlockEvent());
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(true));
  unlock_manager_->OnUnlockEventSent(true);
}

TEST_F(ProximityAuthUnlockManagerImplTest, OnAuthAttempted_SignIn_Success) {
  InitializeTest(false /* multidevice_flags_enabled */);
  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(true));
  CreateUnlockManager(ProximityAuthSystem::SIGN_IN);
  SimulateUserPresentState();

  std::string channel_binding_data = secure_context_.GetChannelBindingData();
  EXPECT_CALL(proximity_auth_client_,
              GetChallengeForUserAndDevice(remote_device_.user_id(),
                                           remote_device_.public_key(),
                                           channel_binding_data, _))
      .WillOnce(Invoke(
          [](const std::string& user_id, const std::string& public_key,
             const std::string& channel_binding_data,
             base::Callback<void(const std::string& challenge)> callback) {
            callback.Run(kChallenge);
          }));

  EXPECT_CALL(messenger_, RequestDecryption(kChallenge));
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(messenger_, DispatchUnlockEvent());
  unlock_manager_->OnDecryptResponse(kSignInSecret);

  EXPECT_CALL(proximity_auth_client_, FinalizeSignin(kSignInSecret));
  unlock_manager_->OnUnlockEventSent(true);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_SignIn_Success_MultiDeviceApiEnabled) {
  InitializeTest(true /* multidevice_flags_enabled */);

  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(true));
  CreateUnlockManager(ProximityAuthSystem::SIGN_IN);
  SimulateUserPresentState();

  std::string channel_binding_data = "channel binding data";

  EXPECT_CALL(proximity_auth_client_,
              GetChallengeForUserAndDevice(remote_device_.user_id(),
                                           remote_device_.public_key(),
                                           channel_binding_data, _))
      .WillOnce(Invoke(
          [](const std::string& user_id, const std::string& public_key,
             const std::string& channel_binding_data,
             base::Callback<void(const std::string& challenge)> callback) {
            callback.Run(kChallenge);
          }));

  EXPECT_CALL(messenger_, RequestDecryption(kChallenge));
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  std::vector<chromeos::secure_channel::mojom::ConnectionCreationDetail>
      creation_details{
          chromeos::secure_channel::mojom::ConnectionCreationDetail::
              REMOTE_DEVICE_USED_BACKGROUND_BLE_ADVERTISING};
  chromeos::secure_channel::mojom::ConnectionMetadataPtr
      connection_metadata_ptr =
          chromeos::secure_channel::mojom::ConnectionMetadata::New(
              creation_details, nullptr /* bluetooth_connection_metadata */,
              channel_binding_data);
  fake_client_channel_->InvokePendingGetConnectionMetadataCallback(
      std::move(connection_metadata_ptr));

  EXPECT_CALL(messenger_, DispatchUnlockEvent());
  unlock_manager_->OnDecryptResponse(kSignInSecret);

  EXPECT_CALL(proximity_auth_client_, FinalizeSignin(kSignInSecret));
  unlock_manager_->OnUnlockEventSent(true);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_SignIn_UnlockEventSendFails) {
  InitializeTest(false /* multidevice_flags_enabled */);
  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(true));
  CreateUnlockManager(ProximityAuthSystem::SIGN_IN);
  SimulateUserPresentState();

  std::string channel_binding_data = secure_context_.GetChannelBindingData();
  EXPECT_CALL(proximity_auth_client_,
              GetChallengeForUserAndDevice(remote_device_.user_id(),
                                           remote_device_.public_key(),
                                           channel_binding_data, _))
      .WillOnce(Invoke(
          [](const std::string& user_id, const std::string& public_key,
             const std::string& channel_binding_data,
             base::Callback<void(const std::string& challenge)> callback) {
            callback.Run(kChallenge);
          }));

  EXPECT_CALL(messenger_, RequestDecryption(kChallenge));
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(messenger_, DispatchUnlockEvent());
  unlock_manager_->OnDecryptResponse(kSignInSecret);

  EXPECT_CALL(proximity_auth_client_, FinalizeSignin(std::string()));
  unlock_manager_->OnUnlockEventSent(false);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_SignIn_DecryptRequestFails) {
  InitializeTest(false /* multidevice_flags_enabled */);
  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(true));
  CreateUnlockManager(ProximityAuthSystem::SIGN_IN);
  SimulateUserPresentState();

  std::string channel_binding_data = secure_context_.GetChannelBindingData();
  EXPECT_CALL(proximity_auth_client_,
              GetChallengeForUserAndDevice(remote_device_.user_id(),
                                           remote_device_.public_key(),
                                           channel_binding_data, _))
      .WillOnce(Invoke(
          [](const std::string& user_id, const std::string& public_key,
             const std::string& channel_binding_data,
             base::Callback<void(const std::string& challenge)> callback) {
            callback.Run(kChallenge);
          }));

  EXPECT_CALL(messenger_, RequestDecryption(kChallenge));
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(proximity_auth_client_, FinalizeSignin(std::string()));
  unlock_manager_->OnDecryptResponse(std::string());
}

}  // namespace proximity_auth
