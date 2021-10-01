// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/device_name_manager_impl.h"

#include "base/test/task_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace bluetooth_config {
namespace {
const char kTestDeviceId[] = "device_id";
const char kTestNickname[] = "nickname";
}  // namespace

class DeviceNameManagerImplTest : public testing::Test {
 protected:
  DeviceNameManagerImplTest() = default;
  DeviceNameManagerImplTest(const DeviceNameManagerImplTest&) = delete;
  DeviceNameManagerImplTest& operator=(const DeviceNameManagerImplTest&) =
      delete;
  ~DeviceNameManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    DeviceNameManagerImpl::RegisterPrefs(test_pref_service_.registry());
  }

  sync_preferences::TestingPrefServiceSyncable* test_pref_service() {
    return &test_pref_service_;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
};

TEST_F(DeviceNameManagerImplTest, GetThenSetValidThenSetInvalid) {
  std::unique_ptr<DeviceNameManagerImpl> manager =
      std::make_unique<DeviceNameManagerImpl>(test_pref_service());
  EXPECT_FALSE(manager->GetDeviceNickname(kTestDeviceId));

  manager->SetDeviceNickname(kTestDeviceId, kTestNickname);
  EXPECT_EQ(manager->GetDeviceNickname(kTestDeviceId), kTestNickname);

  // Set an empty nickname, this should fail and the nickname should be
  // unchanged.
  manager->SetDeviceNickname(kTestDeviceId, "");
  EXPECT_EQ(manager->GetDeviceNickname(kTestDeviceId), kTestNickname);

  // Set nickname above character limit, this should also fail and the nickname
  // should be unchanged.
  manager->SetDeviceNickname(kTestDeviceId,
                             "123456789012345678901234567890123");
  EXPECT_EQ(manager->GetDeviceNickname(kTestDeviceId), kTestNickname);
}

TEST_F(DeviceNameManagerImplTest, NicknameIsPersistedBetweenManagerInstances) {
  std::unique_ptr<DeviceNameManagerImpl> manager =
      std::make_unique<DeviceNameManagerImpl>(test_pref_service());
  EXPECT_FALSE(manager->GetDeviceNickname(kTestDeviceId));

  manager->SetDeviceNickname(kTestDeviceId, kTestNickname);
  EXPECT_EQ(manager->GetDeviceNickname(kTestDeviceId), kTestNickname);

  // Create a new manager and destroy the old one.
  manager = std::make_unique<DeviceNameManagerImpl>(test_pref_service());
  EXPECT_EQ(manager->GetDeviceNickname(kTestDeviceId), kTestNickname);
}

}  // namespace bluetooth_config
}  // namespace chromeos
