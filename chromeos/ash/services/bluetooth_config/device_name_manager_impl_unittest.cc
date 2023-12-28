// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/device_name_manager_impl.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "device/bluetooth/floss/floss_features.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::bluetooth_config {

namespace {

using NiceMockDevice =
    std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>;

const uint32_t kTestBluetoothClass = 1337u;
const char kTestBluetoothName[] = "testName";
const char kTestIdBluez[] = "/org/bluez/hci0/dev_7C_96_D2_8B_FB_17";
const char kTestIdFloss0[] = "7C:96:D2:8B:FB:17";
const char kTestIdFloss1[] = "C7:69:2D:B8:BF:71";
const char kTestNickname[] = "nickname";

class FakeObserver : public DeviceNameManager::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_device_nickname_changed_calls() const {
    return num_device_nickname_changed_calls_;
  }

  const std::string& last_device_id_nickname_changed() const {
    return last_device_id_nickname_changed_;
  }

  const std::optional<std::string>& last_device_nickname_changed() const {
    return last_device_nickname_changed_;
  }

 private:
  // DeviceNameManager::Observer:
  void OnDeviceNicknameChanged(
      const std::string& device_id,
      const std::optional<std::string>& nickname) override {
    ++num_device_nickname_changed_calls_;
    last_device_id_nickname_changed_ = device_id;
    last_device_nickname_changed_ = nickname;
  }

  size_t num_device_nickname_changed_calls_ = 0u;
  std::string last_device_id_nickname_changed_;
  std::optional<std::string> last_device_nickname_changed_;
};

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
    DeviceNameManagerImpl::RegisterLocalStatePrefs(
        test_pref_service_.registry());

    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_adapter_, GetDevices())
        .WillByDefault(
            testing::Invoke(this, &DeviceNameManagerImplTest::GetMockDevices));
  }

  std::unique_ptr<DeviceNameManagerImpl> CreateDeviceNameManager() {
    auto device_name_manager =
        std::make_unique<DeviceNameManagerImpl>(mock_adapter_);
    device_name_manager->AddObserver(&fake_observer_);
    device_name_manager->SetPrefs(&test_pref_service_);
    return device_name_manager;
  }

  void AddDevice(std::string* id_out) {
    // We use the number of devices created in this test as the address.
    std::string address = base::NumberToString(num_devices_created_);
    ++num_devices_created_;

    // Mock devices have their ID set to "${address}-Identifier".
    *id_out = base::StrCat({address, "-Identifier"});

    auto mock_device =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            mock_adapter_.get(), kTestBluetoothClass, kTestBluetoothName,
            address, /*paired=*/false, /*connected=*/false);
    mock_devices_.push_back(std::move(mock_device));
  }

  size_t GetNumDeviceNicknameObserverEvents() const {
    return fake_observer_.num_device_nickname_changed_calls();
  }

  const std::string& GetLastDeviceIdNicknameChanged() const {
    return fake_observer_.last_device_id_nickname_changed();
  }

  const std::optional<std::string>& GetLastDeviceNicknameChanged() const {
    return fake_observer_.last_device_nickname_changed();
  }

  sync_preferences::TestingPrefServiceSyncable* local_state() {
    return &test_pref_service_;
  }

  base::HistogramTester histogram_tester;

 private:
  std::vector<raw_ptr<const device::BluetoothDevice, VectorExperimental>>
  GetMockDevices() {
    std::vector<raw_ptr<const device::BluetoothDevice, VectorExperimental>>
        devices;
    for (auto& device : mock_devices_)
      devices.push_back(device.get());
    return devices;
  }

  base::test::TaskEnvironment task_environment_;

  std::vector<NiceMockDevice> mock_devices_;
  size_t num_devices_created_ = 0u;

  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  FakeObserver fake_observer_;
};

TEST_F(DeviceNameManagerImplTest, GetThenSetValidThenSetInvalidFlossDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(floss::features::kFlossEnabled);
  EXPECT_FALSE(floss::features::IsFlossEnabled());

  std::string device_id;
  AddDevice(&device_id);
  std::unique_ptr<DeviceNameManagerImpl> manager = CreateDeviceNameManager();
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));

  manager->SetDeviceNickname(device_id, kTestNickname);
  EXPECT_EQ(manager->GetDeviceNickname(device_id), kTestNickname);
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 1u);
  EXPECT_EQ(GetLastDeviceIdNicknameChanged(), device_id);
  EXPECT_EQ(GetLastDeviceNicknameChanged(), kTestNickname);

  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.SetNickname.Result",
      device::SetNicknameResult::kInvalidNicknameFormat, 0);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.SetNickname.Result",
                                     device::SetNicknameResult::kSuccess, 1);

  // Set an empty nickname, this should fail and the nickname should be
  // unchanged.
  manager->SetDeviceNickname(device_id, "");
  EXPECT_EQ(manager->GetDeviceNickname(device_id), kTestNickname);
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 1u);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.SetNickname.Result",
      device::SetNicknameResult::kInvalidNicknameFormat, 1);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.SetNickname.Result",
                                     device::SetNicknameResult::kSuccess, 1);

  // Set nickname above character limit, this should also fail and the nickname
  // should be unchanged.
  manager->SetDeviceNickname(device_id, "123456789012345678901234567890123");
  EXPECT_EQ(manager->GetDeviceNickname(device_id), kTestNickname);
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 1u);

  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.SetNickname.Result",
      device::SetNicknameResult::kInvalidNicknameFormat, 2);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.SetNickname.Result",
                                     device::SetNicknameResult::kSuccess, 1);
}

TEST_F(DeviceNameManagerImplTest,
       NicknameIsPersistedBetweenManagerInstancesFlossDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(floss::features::kFlossEnabled);
  EXPECT_FALSE(floss::features::IsFlossEnabled());

  std::string device_id;
  AddDevice(&device_id);
  std::unique_ptr<DeviceNameManagerImpl> manager = CreateDeviceNameManager();
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));

  manager->SetDeviceNickname(device_id, kTestNickname);
  EXPECT_EQ(manager->GetDeviceNickname(device_id), kTestNickname);
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 1u);
  EXPECT_EQ(GetLastDeviceIdNicknameChanged(), device_id);
  EXPECT_EQ(GetLastDeviceNicknameChanged(), kTestNickname);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.SetNickname.Result",
      device::SetNicknameResult::kInvalidNicknameFormat, 0);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.SetNickname.Result",
                                     device::SetNicknameResult::kSuccess, 1);

  // Create a new manager and destroy the old one.
  manager = CreateDeviceNameManager();
  EXPECT_EQ(manager->GetDeviceNickname(device_id), kTestNickname);
}

TEST_F(DeviceNameManagerImplTest, SetNicknameDeviceNotFoundFlossDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(floss::features::kFlossEnabled);
  EXPECT_FALSE(floss::features::IsFlossEnabled());

  const std::string device_id = "device_id";
  std::unique_ptr<DeviceNameManagerImpl> manager = CreateDeviceNameManager();
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));

  // Setting the nickname of an unknown device should fail.
  manager->SetDeviceNickname(device_id, kTestNickname);
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 0u);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.SetNickname.Result",
                                     device::SetNicknameResult::kDeviceNotFound,
                                     1);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.SetNickname.Result",
                                     device::SetNicknameResult::kSuccess, 0);
}

TEST_F(DeviceNameManagerImplTest, RemoveThenSetThenRemoveFlossDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(floss::features::kFlossEnabled);
  EXPECT_FALSE(floss::features::IsFlossEnabled());

  std::string device_id;
  AddDevice(&device_id);
  std::unique_ptr<DeviceNameManagerImpl> manager = CreateDeviceNameManager();
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));

  // Nothing should happen when removing a nickname that doesn't exist.
  manager->RemoveDeviceNickname(device_id);
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 0u);
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));

  manager->SetDeviceNickname(device_id, kTestNickname);
  EXPECT_EQ(manager->GetDeviceNickname(device_id), kTestNickname);
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 1u);
  EXPECT_EQ(GetLastDeviceIdNicknameChanged(), device_id);
  EXPECT_EQ(GetLastDeviceNicknameChanged(), kTestNickname);

  manager->RemoveDeviceNickname(device_id);
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 2u);
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));
  EXPECT_EQ(GetLastDeviceIdNicknameChanged(), device_id);
  EXPECT_EQ(GetLastDeviceNicknameChanged(), std::nullopt);

  // Create a new manager and destroy the old one.
  manager = CreateDeviceNameManager();
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));
}

TEST_F(DeviceNameManagerImplTest, GetThenSetValidThenSetInvalidFlossEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(floss::features::kFlossEnabled);
  EXPECT_TRUE(floss::features::IsFlossEnabled());

  std::string device_id;
  AddDevice(&device_id);
  std::unique_ptr<DeviceNameManagerImpl> manager = CreateDeviceNameManager();
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));

  manager->SetDeviceNickname(device_id, kTestNickname);
  EXPECT_EQ(manager->GetDeviceNickname(device_id), kTestNickname);
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 1u);
  EXPECT_EQ(GetLastDeviceIdNicknameChanged(), device_id);
  EXPECT_EQ(GetLastDeviceNicknameChanged(), kTestNickname);

  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.SetNickname.Result",
      device::SetNicknameResult::kInvalidNicknameFormat, 0);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.SetNickname.Result",
                                     device::SetNicknameResult::kSuccess, 1);

  // Set an empty nickname, this should fail and the nickname should be
  // unchanged.
  manager->SetDeviceNickname(device_id, "");
  EXPECT_EQ(manager->GetDeviceNickname(device_id), kTestNickname);
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 1u);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.SetNickname.Result",
      device::SetNicknameResult::kInvalidNicknameFormat, 1);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.SetNickname.Result",
                                     device::SetNicknameResult::kSuccess, 1);

  // Set nickname above character limit, this should also fail and the nickname
  // should be unchanged.
  manager->SetDeviceNickname(device_id, "123456789012345678901234567890123");
  EXPECT_EQ(manager->GetDeviceNickname(device_id), kTestNickname);
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 1u);

  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.SetNickname.Result",
      device::SetNicknameResult::kInvalidNicknameFormat, 2);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.SetNickname.Result",
                                     device::SetNicknameResult::kSuccess, 1);
}

TEST_F(DeviceNameManagerImplTest,
       NicknameIsPersistedBetweenManagerInstancesFlossEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(floss::features::kFlossEnabled);
  EXPECT_TRUE(floss::features::IsFlossEnabled());

  std::string device_id;
  AddDevice(&device_id);
  std::unique_ptr<DeviceNameManagerImpl> manager = CreateDeviceNameManager();
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));

  manager->SetDeviceNickname(device_id, kTestNickname);
  EXPECT_EQ(manager->GetDeviceNickname(device_id), kTestNickname);
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 1u);
  EXPECT_EQ(GetLastDeviceIdNicknameChanged(), device_id);
  EXPECT_EQ(GetLastDeviceNicknameChanged(), kTestNickname);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.SetNickname.Result",
      device::SetNicknameResult::kInvalidNicknameFormat, 0);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.SetNickname.Result",
                                     device::SetNicknameResult::kSuccess, 1);

  // Create a new manager and destroy the old one.
  manager = CreateDeviceNameManager();
  EXPECT_EQ(manager->GetDeviceNickname(device_id), kTestNickname);
}

TEST_F(DeviceNameManagerImplTest, SetNicknameDeviceNotFoundFlossEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(floss::features::kFlossEnabled);
  EXPECT_TRUE(floss::features::IsFlossEnabled());

  const std::string device_id = "device_id";
  std::unique_ptr<DeviceNameManagerImpl> manager = CreateDeviceNameManager();
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));

  // Setting the nickname of an unknown device should fail.
  manager->SetDeviceNickname(device_id, kTestNickname);
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 0u);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.SetNickname.Result",
                                     device::SetNicknameResult::kDeviceNotFound,
                                     1);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.SetNickname.Result",
                                     device::SetNicknameResult::kSuccess, 0);
}

TEST_F(DeviceNameManagerImplTest, RemoveThenSetThenRemoveFlossEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(floss::features::kFlossEnabled);
  EXPECT_TRUE(floss::features::IsFlossEnabled());

  std::string device_id;
  AddDevice(&device_id);
  std::unique_ptr<DeviceNameManagerImpl> manager = CreateDeviceNameManager();
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));

  // Nothing should happen when removing a nickname that doesn't exist.
  manager->RemoveDeviceNickname(device_id);
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 0u);
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));

  manager->SetDeviceNickname(device_id, kTestNickname);
  EXPECT_EQ(manager->GetDeviceNickname(device_id), kTestNickname);
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 1u);
  EXPECT_EQ(GetLastDeviceIdNicknameChanged(), device_id);
  EXPECT_EQ(GetLastDeviceNicknameChanged(), kTestNickname);

  manager->RemoveDeviceNickname(device_id);
  EXPECT_EQ(GetNumDeviceNicknameObserverEvents(), 2u);
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));
  EXPECT_EQ(GetLastDeviceIdNicknameChanged(), device_id);
  EXPECT_EQ(GetLastDeviceNicknameChanged(), std::nullopt);

  // Create a new manager and destroy the old one.
  manager = CreateDeviceNameManager();
  EXPECT_FALSE(manager->GetDeviceNickname(device_id));
}

TEST_F(DeviceNameManagerImplTest, Migration_DisablingClearsPrefs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(floss::features::kFlossEnabled);
  EXPECT_FALSE(floss::features::IsFlossEnabled());

  // Set the pref to some arbitrary value since we just want to confirm it will
  // be cleared when the feature flag is disabled.
  local_state()->Set(DeviceNameManager::kDeviceIdToNicknameMapPrefName,
                     base::Value(base::Value::Dict()));
  EXPECT_TRUE(local_state()->HasPrefPath(
      DeviceNameManager::kDeviceIdToNicknameMapPrefName));
  CreateDeviceNameManager();
  EXPECT_FALSE(local_state()->HasPrefPath(
      DeviceNameManager::kDeviceIdToNicknameMapPrefName));
}

TEST_F(DeviceNameManagerImplTest, Migration_MigrationHappensOnce) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(floss::features::kFlossEnabled);
  EXPECT_TRUE(floss::features::IsFlossEnabled());

  // The value that we will set the existing/pre-migration prefs to.
  auto existing_prefs = base::Value::Dict().Set(kTestIdBluez, kTestNickname);
  local_state()->Set(DeviceNameManager::kDeviceIdToNicknameMapPrefNameLegacy,
                     base::Value(existing_prefs.Clone()));

  // Set the pref to some arbitrary value since we just want to confirm that if
  // the pref has a value we will assume that we have already performed the
  // migration and will not attempt another migration.
  base::Value::Dict new_prefs;
  local_state()->Set(DeviceNameManager::kDeviceIdToNicknameMapPrefName,
                     base::Value(new_prefs.Clone()));

  EXPECT_TRUE(local_state()->HasPrefPath(
      DeviceNameManager::kDeviceIdToNicknameMapPrefName));

  CreateDeviceNameManager();

  const base::Value::Dict& migrated_prefs =
      local_state()->GetDict(DeviceNameManager::kDeviceIdToNicknameMapPrefName);
  EXPECT_EQ(new_prefs, migrated_prefs);
}

TEST_F(DeviceNameManagerImplTest, Migration) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(floss::features::kFlossEnabled);
  EXPECT_TRUE(floss::features::IsFlossEnabled());

  // The value that we expect to be migrated.
  auto existing_prefs = base::Value::Dict()
                            .Set(kTestIdBluez, kTestNickname)
                            .Set(kTestIdFloss1, kTestNickname);

  local_state()->Set(DeviceNameManager::kDeviceIdToNicknameMapPrefNameLegacy,
                     base::Value(existing_prefs.Clone()));

  EXPECT_FALSE(local_state()->HasPrefPath(
      DeviceNameManager::kDeviceIdToNicknameMapPrefName));

  CreateDeviceNameManager();

  auto migrated_prefs = base::Value::Dict()
                            .Set(kTestIdFloss0, kTestNickname)
                            .Set(kTestIdFloss1, kTestNickname);
  EXPECT_EQ(migrated_prefs,
            local_state()->GetDict(
                DeviceNameManager::kDeviceIdToNicknameMapPrefName));
}

}  // namespace ash::bluetooth_config
