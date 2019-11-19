// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/proximity_auth_system.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/components/proximity_auth/fake_lock_handler.h"
#include "chromeos/components/proximity_auth/fake_remote_device_life_cycle.h"
#include "chromeos/components/proximity_auth/mock_proximity_auth_client.h"
#include "chromeos/components/proximity_auth/proximity_auth_profile_pref_manager.h"
#include "chromeos/components/proximity_auth/switches.h"
#include "chromeos/components/proximity_auth/unlock_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
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
    const chromeos::multidevice::RemoteDeviceRefList& list1,
    const chromeos::multidevice::RemoteDeviceRefList& list2) {
  ASSERT_EQ(list1.size(), list2.size());
  for (size_t i = 0; i < list1.size(); ++i) {
    chromeos::multidevice::RemoteDeviceRef device1 = list1[i];
    chromeos::multidevice::RemoteDeviceRef device2 = list2[i];
    EXPECT_EQ(device1.public_key(), device2.public_key());
  }
}

// Creates a RemoteDeviceRef object for |user_id| with |name|.
chromeos::multidevice::RemoteDeviceRef CreateRemoteDevice(
    const std::string& user_id,
    const std::string& name) {
  return chromeos::multidevice::RemoteDeviceRefBuilder()
      .SetUserId(user_id)
      .SetName(name)
      .Build();
}

// Mock implementation of UnlockManager.
class MockUnlockManager : public UnlockManager {
 public:
  MockUnlockManager() {}
  ~MockUnlockManager() override {}
  MOCK_METHOD0(IsUnlockAllowed, bool());
  MOCK_METHOD1(SetRemoteDeviceLifeCycle, void(RemoteDeviceLifeCycle*));
  MOCK_METHOD1(OnAuthAttempted, void(mojom::AuthType));
  MOCK_METHOD0(CancelConnectionAttempt, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockUnlockManager);
};

// Mock implementation of ProximityAuthProfilePrefManager.
class MockProximityAuthPrefManager : public ProximityAuthProfilePrefManager {
 public:
  MockProximityAuthPrefManager(
      chromeos::multidevice_setup::FakeMultiDeviceSetupClient*
          fake_multidevice_setup_client)
      : ProximityAuthProfilePrefManager(nullptr,
                                        fake_multidevice_setup_client) {}
  ~MockProximityAuthPrefManager() override {}
  MOCK_CONST_METHOD0(GetLastPasswordEntryTimestampMs, int64_t());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProximityAuthPrefManager);
};

// Harness for ProximityAuthSystem to make it testable.
class TestableProximityAuthSystem : public ProximityAuthSystem {
 public:
  TestableProximityAuthSystem(
      chromeos::secure_channel::SecureChannelClient* secure_channel_client,
      std::unique_ptr<UnlockManager> unlock_manager,
      ProximityAuthPrefManager* pref_manager)
      : ProximityAuthSystem(secure_channel_client, std::move(unlock_manager)),
        life_cycle_(nullptr) {}
  ~TestableProximityAuthSystem() override {}

  FakeRemoteDeviceLifeCycle* life_cycle() { return life_cycle_; }

 private:
  std::unique_ptr<RemoteDeviceLifeCycle> CreateRemoteDeviceLifeCycle(
      chromeos::multidevice::RemoteDeviceRef remote_device,
      base::Optional<chromeos::multidevice::RemoteDeviceRef> local_device)
      override {
    std::unique_ptr<FakeRemoteDeviceLifeCycle> life_cycle(
        new FakeRemoteDeviceLifeCycle(remote_device, local_device));
    life_cycle_ = life_cycle.get();
    return std::move(life_cycle);
  }

  FakeRemoteDeviceLifeCycle* life_cycle_;

  DISALLOW_COPY_AND_ASSIGN(TestableProximityAuthSystem);
};

}  // namespace

class ProximityAuthSystemTest : public testing::Test {
 protected:
  ProximityAuthSystemTest()
      : user1_local_device_(CreateRemoteDevice(kUser1, "user1_local_device")),
        user2_local_device_(CreateRemoteDevice(kUser2, "user2_local_device")),
        task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_handle_(task_runner_) {}

  void TearDown() override {
    UnlockScreen();
    pref_manager_.reset();
  }

  void SetUp() override {
    fake_multidevice_setup_client_ = std::make_unique<
        chromeos::multidevice_setup::FakeMultiDeviceSetupClient>();
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
        std::make_unique<chromeos::secure_channel::FakeSecureChannelClient>();

    proximity_auth_system_.reset(new TestableProximityAuthSystem(
        fake_secure_channel_client_.get(), std::move(unlock_manager),
        pref_manager_.get()));

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

  void FocusUser(const std::string& user_id) {
    ScreenlockBridge::Get()->SetFocusedUser(AccountId::FromUserEmail(user_id));
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
  std::unique_ptr<chromeos::secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<TestableProximityAuthSystem> proximity_auth_system_;
  MockUnlockManager* unlock_manager_;
  std::unique_ptr<MockProximityAuthPrefManager> pref_manager_;
  std::unique_ptr<chromeos::multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;

  chromeos::multidevice::RemoteDeviceRef user1_local_device_;
  chromeos::multidevice::RemoteDeviceRef user2_local_device_;

  chromeos::multidevice::RemoteDeviceRefList user1_remote_devices_;
  chromeos::multidevice::RemoteDeviceRefList user2_remote_devices_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;

 private:
  chromeos::multidevice::ScopedDisableLoggingForTesting disable_logging_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthSystemTest);
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
      chromeos::multidevice::RemoteDeviceRefList(),
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
  EXPECT_EQ(kUser1, life_cycle()->GetRemoteDevice().user_id());

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
  EXPECT_EQ(kUser1, life_cycle1->GetRemoteDevice().user_id());
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
  EXPECT_EQ(kUser2, life_cycle2->GetRemoteDevice().user_id());
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
  EXPECT_EQ(kUser1, life_cycle->GetRemoteDevice().user_id());

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
  EXPECT_EQ(kUser1, life_cycle->GetRemoteDevice().user_id());

  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
      .Times(AtLeast(1));
}

TEST_F(ProximityAuthSystemTest, ToggleFocus_SameUserRefocused) {
  RemoteDeviceLifeCycle* life_cycle = nullptr;
  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(_))
      .WillOnce(SaveArg<0>(&life_cycle));
  FocusUser(kUser1);
  EXPECT_EQ(kUser1, life_cycle->GetRemoteDevice().user_id());

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
  EXPECT_EQ(kUser1, life_cycle()->GetRemoteDevice().user_id());
}

TEST_F(ProximityAuthSystemTest, OnAuthAttempted) {
  FocusUser(kUser1);
  EXPECT_CALL(*unlock_manager_, OnAuthAttempted(_));
  proximity_auth_system_->OnAuthAttempted(AccountId::FromUserEmail(kUser1));
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

  EXPECT_EQ(kUser1, life_cycle()->GetRemoteDevice().user_id());

  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
      .Times(AtLeast(1));
}

}  // namespace proximity_auth
