// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/multidevice/remote_device_cache.h"

#include <algorithm>

#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::multidevice {

class RemoteDeviceCacheTest : public testing::Test {
 public:
  RemoteDeviceCacheTest(const RemoteDeviceCacheTest&) = delete;
  RemoteDeviceCacheTest& operator=(const RemoteDeviceCacheTest&) = delete;

 protected:
  RemoteDeviceCacheTest()
      : test_remote_device_list_(CreateRemoteDeviceListForTest(5)),
        test_remote_device_ref_list_(CreateRemoteDeviceRefListForTest(5)) {}

  // testing::Test:
  void SetUp() override { cache_ = RemoteDeviceCache::Factory::Create(); }

  void VerifyCacheRemoteDevices(
      RemoteDeviceRefList expected_remote_device_ref_list) {
    RemoteDeviceRefList remote_device_ref_list = cache_->GetRemoteDevices();
    std::sort(remote_device_ref_list.begin(), remote_device_ref_list.end(),
              [](const auto& device_1, const auto& device_2) {
                return device_1 < device_2;
              });

    EXPECT_EQ(expected_remote_device_ref_list, remote_device_ref_list);
  }

  RemoteDeviceList test_remote_device_list_;
  const RemoteDeviceRefList test_remote_device_ref_list_;
  std::unique_ptr<RemoteDeviceCache> cache_;
};

TEST_F(RemoteDeviceCacheTest, TestNoRemoteDevices) {
  VerifyCacheRemoteDevices(RemoteDeviceRefList());
  EXPECT_EQ(std::nullopt, cache_->GetRemoteDevice(
                              test_remote_device_ref_list_[0].instance_id(),
                              test_remote_device_ref_list_[0].GetDeviceId()));
}

TEST_F(RemoteDeviceCacheTest, TestSetAndGetRemoteDevices) {
  cache_->SetRemoteDevices(test_remote_device_list_);

  VerifyCacheRemoteDevices(test_remote_device_ref_list_);
  EXPECT_EQ(
      test_remote_device_ref_list_[0],
      cache_->GetRemoteDevice(test_remote_device_ref_list_[0].instance_id(),
                              test_remote_device_ref_list_[0].GetDeviceId()));
}

TEST_F(RemoteDeviceCacheTest,
       TestSetAndGetRemoteDevices_LookupByInstanceIdOrPublicKey) {
  test_remote_device_list_[0].instance_id.clear();
  test_remote_device_list_[1].public_key.clear();
  cache_->SetRemoteDevices(test_remote_device_list_);

  GetMutableRemoteDevice(test_remote_device_ref_list_[0])->instance_id.clear();
  GetMutableRemoteDevice(test_remote_device_ref_list_[1])->public_key.clear();
  VerifyCacheRemoteDevices(test_remote_device_ref_list_);

  EXPECT_EQ(
      test_remote_device_ref_list_[0],
      cache_->GetRemoteDevice(std::nullopt /* instance_id */,
                              test_remote_device_ref_list_[0].GetDeviceId()));
  EXPECT_EQ(
      test_remote_device_ref_list_[1],
      cache_->GetRemoteDevice(test_remote_device_ref_list_[1].instance_id(),
                              std::nullopt /* legacy_device_id */));
  EXPECT_EQ(
      test_remote_device_ref_list_[2],
      cache_->GetRemoteDevice(std::nullopt /* instance_id */,
                              test_remote_device_ref_list_[2].GetDeviceId()));
  EXPECT_EQ(
      test_remote_device_ref_list_[2],
      cache_->GetRemoteDevice(test_remote_device_ref_list_[2].instance_id(),
                              std::nullopt /* legacy_device_id */));
}

TEST_F(RemoteDeviceCacheTest,
       TestSetRemoteDevices_RemoteDeviceRefsRemainValidAfterCacheRemoval) {
  cache_->SetRemoteDevices(test_remote_device_list_);

  VerifyCacheRemoteDevices(test_remote_device_ref_list_);

  cache_->SetRemoteDevices(RemoteDeviceList());

  VerifyCacheRemoteDevices(test_remote_device_ref_list_);
}

TEST_F(RemoteDeviceCacheTest,
       TestSetRemoteDevices_RemoteDeviceRefsRemainValidAfterValidCacheUpdate) {
  // Store the device with a last update time of 1000.
  RemoteDevice remote_device = CreateRemoteDeviceForTest();
  remote_device.last_update_time_millis = 1000;
  cache_->SetRemoteDevices({remote_device});

  RemoteDeviceRef remote_device_ref = *cache_->GetRemoteDevice(
      remote_device.instance_id, remote_device.GetDeviceId());
  EXPECT_EQ(remote_device.name, remote_device_ref.name());

  // Update the device's name and update time. Since the incoming remote device
  // has a newer update time, the entry should successfully update.
  remote_device.name = "new name";
  remote_device.last_update_time_millis = 2000;
  cache_->SetRemoteDevices({remote_device});

  EXPECT_EQ(remote_device.name, remote_device_ref.name());
}

TEST_F(RemoteDeviceCacheTest,
       TestSetRemoteDevices_DevicesSharingSameInstanceId) {
  RemoteDevice remote_device = CreateRemoteDeviceForTest();
  cache_->SetRemoteDevices({remote_device});
  EXPECT_EQ(cache_->GetRemoteDevices().size(), 1u);

  // Updatea the instance id but keep device id unchanged.
  remote_device.instance_id = "rAnDOMiNStanceID";

  cache_->SetRemoteDevices({remote_device});
  // New entry should be added successfully.
  EXPECT_EQ(cache_->GetRemoteDevices().size(), 2u);
}

// Currently disabled; will be re-enabled when https://crbug.com/856746 is
// fixed.
TEST_F(
    RemoteDeviceCacheTest,
    DISABLED_TestSetRemoteDevices_RemoteDeviceCacheDoesNotUpdateWithStaleRemoteDevice) {
  // Store the device with a last update time of 1000.
  RemoteDevice remote_device = CreateRemoteDeviceForTest();
  remote_device.last_update_time_millis = 1000;
  cache_->SetRemoteDevices({remote_device});

  RemoteDeviceRef remote_device_ref = *cache_->GetRemoteDevice(
      remote_device.instance_id, remote_device.GetDeviceId());
  EXPECT_EQ(remote_device.name, remote_device_ref.name());

  // Update the device's name and update time, this time reducing the
  // last update time to 500. Since this is less than 1000, adding the
  // device to the cache should not cause it to overwrite the previous
  // entry, since this entry is older.
  std::string prev_name = remote_device.name;
  remote_device.last_update_time_millis = 500;
  remote_device.name = "new name";
  cache_->SetRemoteDevices({remote_device});

  EXPECT_EQ(prev_name, remote_device_ref.name());
}

}  // namespace ash::multidevice
