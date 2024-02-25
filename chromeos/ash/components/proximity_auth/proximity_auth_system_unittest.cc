// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/proximity_auth_system.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/components/proximity_auth/fake_lock_handler.h"
#include "chromeos/ash/components/proximity_auth/fake_remote_device_life_cycle.h"
#include "chromeos/ash/components/proximity_auth/mock_proximity_auth_client.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_profile_pref_manager.h"
#include "chromeos/ash/components/proximity_auth/unlock_manager.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::InSequence;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;

namespace proximity_auth {

namespace {

const char kUser1[] = "user1";
const char kUser2[] = "user2";

void CompareRemoteDeviceRefLists(
    const ash::multidevice::RemoteDeviceRefList& list1,
    const ash::multidevice::RemoteDeviceRefList& list2) {
  ASSERT_EQ(list1.size(), list2.size());
  for (size_t i = 0; i < list1.size(); ++i) {
    ash::multidevice::RemoteDeviceRef device1 = list1[i];
    ash::multidevice::RemoteDeviceRef device2 = list2[i];
    EXPECT_EQ(device1.public_key(), device2.public_key());
  }
}

// Creates a RemoteDeviceRef object for |user_id| with |name|.
ash::multidevice::RemoteDeviceRef CreateRemoteDevice(
    const std::string& user_email,
    const std::string& name) {
  return ash::multidevice::RemoteDeviceRefBuilder()
      .SetUserEmail(user_email)
      .SetName(name)
      .Build();
}

// Mock implementation of UnlockManager.
class MockUnlockManager : public UnlockManager {
 public:
  MockUnlockManager() {}

  MockUnlockManager(const MockUnlockManager&) = delete;
  MockUnlockManager& operator=(const MockUnlockManager&) = delete;

  ~MockUnlockManager() override {}
  MOCK_METHOD0(IsUnlockAllowed, bool());
  MOCK_METHOD1(SetRemoteDeviceLifeCycle, void(RemoteDeviceLifeCycle*));
  MOCK_METHOD1(OnAuthAttempted, void(mojom::AuthType));
  MOCK_METHOD0(CancelConnectionAttempt, void());
  MOCK_METHOD0(GetLastRemoteStatusUnlockForLogging, std::string());
};

// Mock implementation of ProximityAuthProfilePrefManager.
class MockProximityAuthPrefManager : public ProximityAuthProfilePrefManager {
 public:
  MockProximityAuthPrefManager(
      ash::multidevice_setup::FakeMultiDeviceSetupClient*
          fake_multidevice_setup_client)
      : ProximityAuthProfilePrefManager(nullptr,
                                        fake_multidevice_setup_client) {}

  MockProximityAuthPrefManager(const MockProximityAuthPrefManager&) = delete;
  MockProximityAuthPrefManager& operator=(const MockProximityAuthPrefManager&) =
      delete;

  MOCK_CONST_METHOD0(GetLastPasswordEntryTimestampMs, int64_t());
};

// Harness for ProximityAuthSystem to make it testable.
class TestableProximityAuthSystem : public ProximityAuthSystem {
 public:
  TestableProximityAuthSystem(
      ash::secure_channel::SecureChannelClient* secure_channel_client,
      std::unique_ptr<UnlockManager> unlock_manager,
      ProximityAuthProfilePrefManager* pref_manager)
      : ProximityAuthSystem(secure_channel_client, std::move(unlock_manager)),
        life_cycle_(nullptr) {}

  TestableProximityAuthSystem(const TestableProximityAuthSystem&) = delete;
  TestableProximityAuthSystem& operator=(const TestableProximityAuthSystem&) =
      delete;

  ~TestableProximityAuthSystem() override {}

  FakeRemoteDeviceLifeCycle* life_cycle() { return life_cycle_; }

 private:
  std::unique_ptr<RemoteDeviceLifeCycle> CreateRemoteDeviceLifeCycle(
      ash::multidevice::RemoteDeviceRef remote_device,
      std::optional<ash::multidevice::RemoteDeviceRef> local_device) override {
    std::unique_ptr<FakeRemoteDeviceLifeCycle> life_cycle(
        new FakeRemoteDeviceLifeCycle(remote_device, local_device));
    life_cycle_ = life_cycle.get();
    return life_cycle;
  }

  raw_ptr<FakeRemoteDeviceLifeCycle, DanglingUntriaged> life_cycle_;
};

}  // namespace

class ProximityAuthSystemTest : public testing::Test {
 public:
  ProximityAuthSystemTest(const ProximityAuthSystemTest&) = delete;
  ProximityAuthSystemTest& operator=(const ProximityAuthSystemTest&) = delete;

 protected:
  ProximityAuthSystemTest()
      : user1_local_device_(CreateRemoteDevice(kUser1, "user1_local_device")),
        user2_local_device_(CreateRemoteDevice(kUser2, "user2_local_device")),
        task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_current_default_handle_(task_runner_) {}

  void TearDown() override {
    UnlockScreen();
    pref_manager_.reset();
  }

  void SetUp() override {
    fake_multidevice_setup_client_ =
        std::make_unique<ash::multidevice_setup::FakeMultiDeviceSetupClient>();
    pref_manager_ = std::make_unique<NiceMock<MockProximityAuthPrefManager>>(
        fake_multidevice_setup_client_.get());

    user1_remote_devices_.push_back(
        CreateRemoteDevice(kUser1, "user1_device1"));
    user1_remote_devices_.push_back(
        CreateRemoteDevice(kUser1, "user1_device2"));

    user2_remote_devices_.push_back(
        CreateRemoteDevice(kUser2, "user2_device1"));
    user2_remote_devices_.push_back(
        CreateRemoteDevice(kUser2, "user2_device2"));
    user2_remote_devices_.push_back(
        CreateRemoteDevice(kUser2, "user2_device3"));

    std::unique_ptr<MockUnlockManager> unlock_manager(
        new NiceMock<MockUnlockManager>());
    unlock_manager_ = unlock_manager.get();

    fake_secure_channel_client_ =
        std::make_unique<ash::secure_channel::FakeSecureChannelClient>();

    proximity_auth_system_ = std::make_unique<TestableProximityAuthSystem>(
        fake_secure_channel_client_.get(), std::move(unlock_manager),
        pref_manager_.get());

    proximity_auth_system_->SetRemoteDevicesForUser(
        AccountId::FromUserEmail(kUser1), user1_remote_devices_,
        user1_local_device_);
    proximity_auth_system_->Start();
    LockScreen();
  }

  void LockScreen() {
    ScreenlockBridge::Get()->SetFocusedUser(AccountId());
    ScreenlockBridge::Get()->SetLockHandler(&lock_handler_);
  }

  void FocusUser(const std::string& user_email) {
    ScreenlockBridge::Get()->SetFocusedUser(
        AccountId::FromUserEmail(user_email));
  }

  void UnlockScreen() { ScreenlockBridge::Get()->SetLockHandler(nullptr); }

  void SimulateSuspend() {
    proximity_auth_system_->OnSuspend();
    proximity_auth_system_->OnSuspendDone();
    task_runner_->RunUntilIdle();
  }

  FakeRemoteDeviceLifeCycle* life_cycle() {
    return proximity_auth_system_->life_cycle();
  }

  FakeLockHandler lock_handler_;
  NiceMock<MockProximityAuthClient> proximity_auth_client_;
  std::unique_ptr<ash::secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<TestableProximityAuthSystem> proximity_auth_system_;
  raw_ptr<MockUnlockManager> unlock_manager_;
  std::unique_ptr<MockProximityAuthPrefManager> pref_manager_;
  std::unique_ptr<ash::multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;

  ash::multidevice::RemoteDeviceRef user1_local_device_;
  ash::multidevice::RemoteDeviceRef user2_local_device_;

  ash::multidevice::RemoteDeviceRefList user1_remote_devices_;
  ash::multidevice::RemoteDeviceRefList user2_remote_devices_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      thread_task_runner_current_default_handle_;

 private:
  ash::multidevice::ScopedDisableLoggingForTesting disable_logging_;
};

TEST_F(ProximityAuthSystemTest, SetRemoteDevicesForUser_NotStarted) {
  AccountId account1 = AccountId::FromUserEmail(kUser1);
  AccountId account2 = AccountId::FromUserEmail(kUser2);
  proximity_auth_system_->SetRemoteDevicesForUser(
      account1, user1_remote_devices_, user1_local_device_);
  proximity_auth_system_->SetRemoteDevicesForUser(
      account2, user2_remote_devices_, user1_local_device_);

  CompareRemoteDeviceRefLists(
      user1_remote_devices_,
      proximity_auth_system_->GetRemoteDevicesForUser(account1));

  CompareRemoteDeviceRefLists(
      user2_remote_devices_,
      proximity_auth_system_->GetRemoteDevicesForUser(account2));

  CompareRemoteDeviceRefLists(
      ash::multidevice::RemoteDeviceRefList(),
      proximity_auth_system_->GetRemoteDevicesForUser(
          AccountId::FromUserEmail("non_existent_user@google.com")));
}

TEST_F(ProximityAuthSystemTest, SetRemoteDevicesForUser_Started) {
  AccountId account1 = AccountId::FromUserEmail(kUser1);
  AccountId account2 = AccountId::FromUserEmail(kUser2);
  proximity_auth_system_->SetRemoteDevicesForUser(
      account1, user1_remote_devices_, user1_local_device_);
  proximity_auth_system_->Start();
  proximity_auth_system_->SetRemoteDevicesForUser(
      account2, user2_remote_devices_, user2_local_device_);

  CompareRemoteDeviceRefLists(
      user1_remote_devices_,
      proximity_auth_system_->GetRemoteDevicesForUser(account1));

  CompareRemoteDeviceRefLists(
      user2_remote_devices_,
      proximity_auth_system_->GetRemoteDevicesForUser(account2));
}

TEST_F(ProximityAuthSystemTest, FocusRegisteredUser) {
  EXPECT_FALSE(life_cycle());
  EXPECT_EQ(std::string(),
            ScreenlockBridge::Get()->focused_account_id().GetUserEmail());

  RemoteDeviceLifeCycle* unlock_manager_life_cycle = nullptr;
  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(_))
      .WillOnce(SaveArg<0>(&unlock_manager_life_cycle));
  FocusUser(kUser1);

  EXPECT_EQ(life_cycle(), unlock_manager_life_cycle);
  EXPECT_TRUE(life_cycle());
  EXPECT_FALSE(life_cycle()->started());
  EXPECT_EQ(kUser1, life_cycle()->GetRemoteDevice().user_email());

  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
      .Times(AtLeast(1));
}

TEST_F(ProximityAuthSystemTest, FocusUnregisteredUser) {
  EXPECT_FALSE(life_cycle());
  EXPECT_EQ(std::string(),
            ScreenlockBridge::Get()->focused_account_id().GetUserEmail());
  EXPECT_FALSE(life_cycle());

  FocusUser(kUser2);
  EXPECT_FALSE(life_cycle());
}

TEST_F(ProximityAuthSystemTest, ToggleFocus_RegisteredUsers) {
  proximity_auth_system_->SetRemoteDevicesForUser(
      AccountId::FromUserEmail(kUser1), user1_remote_devices_,
      user1_local_device_);
  proximity_auth_system_->SetRemoteDevicesForUser(
      AccountId::FromUserEmail(kUser2), user2_remote_devices_,
      user2_local_device_);

  RemoteDeviceLifeCycle* life_cycle1 = nullptr;
  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(_))
      .WillOnce(SaveArg<0>(&life_cycle1));
  FocusUser(kUser1);
  EXPECT_EQ(kUser1, life_cycle1->GetRemoteDevice().user_email());
  EXPECT_EQ(user1_local_device_, life_cycle()->local_device());

  RemoteDeviceLifeCycle* life_cycle2 = nullptr;
  {
    InSequence sequence;
    EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
        .Times(AtLeast(1));
    EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(_))
        .WillOnce(SaveArg<0>(&life_cycle2));
  }
  FocusUser(kUser2);
  EXPECT_EQ(kUser2, life_cycle2->GetRemoteDevice().user_email());
  EXPECT_EQ(user2_local_device_, life_cycle()->local_device());

  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
      .Times(AtLeast(1));
}

TEST_F(ProximityAuthSystemTest, ToggleFocus_UnregisteredUsers) {
  FocusUser(kUser2);
  EXPECT_FALSE(life_cycle());

  FocusUser("unregistered-user");
  EXPECT_FALSE(life_cycle());

  FocusUser(kUser2);
  EXPECT_FALSE(life_cycle());
}

TEST_F(ProximityAuthSystemTest, ToggleFocus_RegisteredAndUnregisteredUsers) {
  // Focus User 1, who is registered. This should create a new life cycle.
  RemoteDeviceLifeCycle* life_cycle = nullptr;
  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(_))
      .WillOnce(SaveArg<0>(&life_cycle));
  FocusUser(kUser1);
  EXPECT_EQ(kUser1, life_cycle->GetRemoteDevice().user_email());

  // User 2 has not been registered yet, so focusing them should not create a
  // new life cycle.
  life_cycle = nullptr;
  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr));
  FocusUser(kUser2);
  EXPECT_FALSE(life_cycle);

  // Focusing back to User 1 should recreate a new life cycle.
  life_cycle = nullptr;
  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(_))
      .WillOnce(SaveArg<0>(&life_cycle));
  FocusUser(kUser1);
  EXPECT_EQ(kUser1, life_cycle->GetRemoteDevice().user_email());

  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
      .Times(AtLeast(1));
}

TEST_F(ProximityAuthSystemTest, ToggleFocus_SameUserRefocused) {
  RemoteDeviceLifeCycle* life_cycle = nullptr;
  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(_))
      .WillOnce(SaveArg<0>(&life_cycle));
  FocusUser(kUser1);
  EXPECT_EQ(kUser1, life_cycle->GetRemoteDevice().user_email());

  // Focusing the user again should be idempotent. The screenlock UI may call
  // focus on the same user multiple times.
  // SetRemoteDeviceLifeCycle() is only expected to be called once.
  FocusUser(kUser1);

  // The RemoteDeviceLifeCycle should be nulled upon destruction.
  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
      .Times(AtLeast(1));
}

TEST_F(ProximityAuthSystemTest, RestartSystem_UnregisteredUserFocused) {
  FocusUser(kUser2);

  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
      .Times(AnyNumber());
  proximity_auth_system_->Stop();
  proximity_auth_system_->Start();
  EXPECT_FALSE(life_cycle());
}

TEST_F(ProximityAuthSystemTest, StopSystem_RegisteredUserFocused) {
  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(NotNull()));
  FocusUser(kUser1);

  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
      .Times(AtLeast(1));
  proximity_auth_system_->Stop();

  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(NotNull()));
  proximity_auth_system_->Start();
  EXPECT_EQ(kUser1, life_cycle()->GetRemoteDevice().user_email());
}

TEST_F(ProximityAuthSystemTest, OnAuthAttempted) {
  FocusUser(kUser1);
  EXPECT_CALL(*unlock_manager_, OnAuthAttempted(_));
  proximity_auth_system_->OnAuthAttempted();
}

TEST_F(ProximityAuthSystemTest, Suspend_ScreenUnlocked) {
  UnlockScreen();
  EXPECT_FALSE(life_cycle());
  SimulateSuspend();
  EXPECT_FALSE(life_cycle());
}

TEST_F(ProximityAuthSystemTest, Suspend_UnregisteredUserFocused) {
  SimulateSuspend();
  EXPECT_FALSE(life_cycle());
}

TEST_F(ProximityAuthSystemTest, Suspend_RegisteredUserFocused) {
  FocusUser(kUser1);

  {
    InSequence sequence;
    EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
        .Times(AtLeast(1));
    EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(NotNull()));
    SimulateSuspend();
  }

  EXPECT_EQ(kUser1, life_cycle()->GetRemoteDevice().user_email());

  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
      .Times(AtLeast(1));
}

}  // namespace proximity_auth
