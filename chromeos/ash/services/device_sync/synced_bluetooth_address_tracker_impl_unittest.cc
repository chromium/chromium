// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/synced_bluetooth_address_tracker_impl.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_scheduler.h"
#include "chromeos/ash/services/device_sync/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace device_sync {

namespace {
const char kHasNotSyncedYetPrefValue[] = "hasNotSyncedYet";
const char kDefaultAdapterAddress[] = "01:23:45:67:89:AB";
}  // namespace

class DeviceSyncSyncedBluetoothAddressTrackerImplTest : public testing::Test {
 protected:
  DeviceSyncSyncedBluetoothAddressTrackerImplTest() = default;
  DeviceSyncSyncedBluetoothAddressTrackerImplTest(
      const DeviceSyncSyncedBluetoothAddressTrackerImplTest&) = delete;
  DeviceSyncSyncedBluetoothAddressTrackerImplTest& operator=(
      const DeviceSyncSyncedBluetoothAddressTrackerImplTest&) = delete;
  ~DeviceSyncSyncedBluetoothAddressTrackerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    is_adapter_present_ = true;
    ON_CALL(*mock_adapter_, IsPresent())
        .WillByDefault(
            Invoke(this, &DeviceSyncSyncedBluetoothAddressTrackerImplTest::
                             is_adapter_present));
    ON_CALL(*mock_adapter_, GetAddress())
        .WillByDefault(Invoke(
            this,
            &DeviceSyncSyncedBluetoothAddressTrackerImplTest::adapter_address));
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);

    fake_cryptauth_scheduler_.StartDeviceSyncScheduling(
        fake_device_sync_delegate_.GetWeakPtr());

    SyncedBluetoothAddressTrackerImpl::RegisterPrefs(pref_service_.registry());
  }

  void Initialize(bool is_flag_enabled,
                  const std::string& initial_bluetooth_pref_value) {
    static const std::vector<base::test::FeatureRef> kPhoneHubFeatureVector{
        features::kPhoneHub};
    static const std::vector<base::test::FeatureRef> kNoFeaturesVector;

    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/is_flag_enabled ? kPhoneHubFeatureVector
                                             : kNoFeaturesVector,
        /*disabled_features=*/is_flag_enabled ? kNoFeaturesVector
                                              : kPhoneHubFeatureVector);

    pref_service_.SetString(
        prefs::kCryptAuthBluetoothAddressProvidedDuringLastSync,
        initial_bluetooth_pref_value);

    tracker_ = SyncedBluetoothAddressTrackerImpl::Factory::Create(
        &fake_cryptauth_scheduler_, &pref_service_);
  }

  std::string GetBluetoothAddress() {
    std::string address_to_return;

    base::RunLoop run_loop;
    tracker_->GetBluetoothAddress(
        base::BindLambdaForTesting([&](const std::string& address) {
          address_to_return = address;
          run_loop.Quit();
        }));
    run_loop.Run();

    return address_to_return;
  }

  std::string GetAddressStoredInPrefs() {
    return pref_service_.GetString(
        prefs::kCryptAuthBluetoothAddressProvidedDuringLastSync);
  }

  SyncedBluetoothAddressTracker& tracker() { return *tracker_; }
  const FakeCryptAuthScheduler& fake_cryptauth_scheduler() const {
    return fake_cryptauth_scheduler_;
  }

  void set_adapter_address(const std::string& adapter_address) {
    adapter_address_ = adapter_address;
  }

  void SetAdapterPresentState(bool present) {
    is_adapter_present_ = present;

    if (tracker_) {
      SyncedBluetoothAddressTrackerImpl* impl =
          static_cast<SyncedBluetoothAddressTrackerImpl*>(tracker_.get());
      impl->AdapterPresentChanged(mock_adapter_.get(), present);
    }
  }

 private:
  bool is_adapter_present() { return is_adapter_present_; }
  const std::string& adapter_address() { return adapter_address_; }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;

  FakeCryptAuthScheduler fake_cryptauth_scheduler_;
  FakeCryptAuthSchedulerDeviceSyncDelegate fake_device_sync_delegate_;
  TestingPrefServiceSimple pref_service_;
  bool is_adapter_present_ = true;
  std::string adapter_address_ = kDefaultAdapterAddress;

  std::unique_ptr<SyncedBluetoothAddressTracker> tracker_;
};

TEST_F(DeviceSyncSyncedBluetoothAddressTrackerImplTest, FlagOff) {
  Initialize(/*is_flag_enabled=*/false, kHasNotSyncedYetPrefValue);

  // When the flag is off, an empty string is returned, even though the adapter
  // has the default address.
  EXPECT_TRUE(GetBluetoothAddress().empty());

  // When the flag is off, setting the last synced address does nothing.
  tracker().SetLastSyncedBluetoothAddress(kDefaultAdapterAddress);
  EXPECT_EQ(kHasNotSyncedYetPrefValue, GetAddressStoredInPrefs());

  EXPECT_EQ(0u, fake_cryptauth_scheduler().num_sync_requests());
}

TEST_F(DeviceSyncSyncedBluetoothAddressTrackerImplTest, HastNotSynced) {
  Initialize(/*is_flag_enabled=*/true, kHasNotSyncedYetPrefValue);

  EXPECT_EQ(kDefaultAdapterAddress, GetBluetoothAddress());
  tracker().SetLastSyncedBluetoothAddress(kDefaultAdapterAddress);
  EXPECT_EQ(kDefaultAdapterAddress, GetAddressStoredInPrefs());

  EXPECT_EQ(0u, fake_cryptauth_scheduler().num_sync_requests());
}

TEST_F(DeviceSyncSyncedBluetoothAddressTrackerImplTest, ChangedAddress) {
  // Initialize with a different address from kDefaultAdapterAddress.
  Initialize(/*is_flag_enabled=*/true, "23:45:67:89:AB:CD");

  // Address changed, so a new sync should have been triggered.
  EXPECT_EQ(1u, fake_cryptauth_scheduler().num_sync_requests());
}

TEST_F(DeviceSyncSyncedBluetoothAddressTrackerImplTest, SameAddress) {
  Initialize(/*is_flag_enabled=*/true, kDefaultAdapterAddress);

  // Address has not changed, so no new sync should have been triggered.
  EXPECT_EQ(0u, fake_cryptauth_scheduler().num_sync_requests());
}

TEST_F(DeviceSyncSyncedBluetoothAddressTrackerImplTest, AdapterPresent) {
  // Simulate a user already having synced but having no Bluetooth adapter.
  SetAdapterPresentState(false);
  set_adapter_address(std::string());
  Initialize(/*is_flag_enabled=*/true, std::string());

  // Adapter is not present.
  EXPECT_EQ(0u, fake_cryptauth_scheduler().num_sync_requests());
  EXPECT_TRUE(GetBluetoothAddress().empty());

  // Now, simulate the usr plugging in a USB Bluetooth adapter.
  set_adapter_address(kDefaultAdapterAddress);
  SetAdapterPresentState(true);

  // Address changed, so a new sync should have been triggered.
  EXPECT_EQ(1u, fake_cryptauth_scheduler().num_sync_requests());
}

}  // namespace device_sync

}  // namespace ash
