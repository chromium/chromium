// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/active_devices_provider_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/sync_device_info/device_info_util.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::DeviceInfo;
using syncer::FakeDeviceInfoTracker;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

namespace browser_sync {
namespace {

constexpr int kPulseIntervalMinutes = 60;

std::unique_ptr<DeviceInfo> CreateFakeDeviceInfo(
    const std::string& name,
    const std::string& fcm_registration_token,
    base::Time last_updated_timestamp) {
  return std::make_unique<syncer::DeviceInfo>(
      base::GUID::GenerateRandomV4().AsLowercaseString(), name,
      "chrome_version", "user_agent", sync_pb::SyncEnums::TYPE_UNSET,
      "device_id", "manufacturer_name", "model_name", last_updated_timestamp,
      base::TimeDelta::FromMinutes(kPulseIntervalMinutes),
      /*send_tab_to_self_receiving_enabled=*/false,
      /*sharing_info=*/base::nullopt, fcm_registration_token,
      /*interested_data_types=*/syncer::ModelTypeSet());
}

class ActiveDevicesProviderImplTest : public testing::Test {
 public:
  ActiveDevicesProviderImplTest()
      : active_devices_provider_(&fake_device_info_tracker_, &clock_) {}

  ~ActiveDevicesProviderImplTest() override = default;

  void AddDevice(const std::string& name,
                 const std::string& fcm_registration_token,
                 base::Time last_updated_timestamp) {
    device_list_.push_back(CreateFakeDeviceInfo(name, fcm_registration_token,
                                                last_updated_timestamp));
    fake_device_info_tracker_.Add(device_list_.back().get());
  }

 protected:
  std::vector<std::unique_ptr<DeviceInfo>> device_list_;
  FakeDeviceInfoTracker fake_device_info_tracker_;
  base::SimpleTestClock clock_;
  ActiveDevicesProviderImpl active_devices_provider_;
};

TEST_F(ActiveDevicesProviderImplTest, ShouldFilterInactiveDevices) {
  AddDevice("device_recent", /*fcm_registration_token=*/"",
            clock_.Now() - base::TimeDelta::FromMinutes(1));

  // Should be considered as active device due to margin even though the device
  // is outside the pulse interval.
  AddDevice(
      "device_pulse_interval",
      /*fcm_registration_token=*/"",
      clock_.Now() - base::TimeDelta::FromMinutes(kPulseIntervalMinutes + 1));

  // Very old device.
  AddDevice("device_inactive", /*fcm_registration_token=*/"",
            clock_.Now() - base::TimeDelta::FromDays(100));

  EXPECT_EQ(2u, active_devices_provider_.CountActiveDevicesIfAvailable());
}

TEST_F(ActiveDevicesProviderImplTest, ShouldReturnZeroDevices) {
  EXPECT_EQ(0u, active_devices_provider_.CountActiveDevicesIfAvailable());
}

TEST_F(ActiveDevicesProviderImplTest, ShouldInvokeCallback) {
  base::MockCallback<
      syncer::ActiveDevicesProvider::ActiveDevicesChangedCallback>
      callback;
  active_devices_provider_.SetActiveDevicesChangedCallback(callback.Get());
  EXPECT_CALL(callback, Run());
  active_devices_provider_.OnDeviceInfoChange();
  active_devices_provider_.SetActiveDevicesChangedCallback(
      base::RepeatingClosure());
}

TEST_F(ActiveDevicesProviderImplTest, ShouldReturnActiveFCMRegistrationTokens) {
  AddDevice("device_1", "fcm_token_1",
            clock_.Now() - base::TimeDelta::FromMinutes(1));
  AddDevice("device_2", "fcm_token_2",
            clock_.Now() - base::TimeDelta::FromMinutes(1));
  AddDevice("device_inactive", "fcm_token_3",
            clock_.Now() - base::TimeDelta::FromDays(100));

  ASSERT_EQ(3u, device_list_.size());

  EXPECT_THAT(
      active_devices_provider_.CollectFCMRegistrationTokensForInvalidations(
          "other_guid"),
      UnorderedElementsAre(device_list_[0]->fcm_registration_token(),
                           device_list_[1]->fcm_registration_token()));
  EXPECT_THAT(
      active_devices_provider_.CollectFCMRegistrationTokensForInvalidations(
          device_list_[0]->guid()),
      UnorderedElementsAre(device_list_[1]->fcm_registration_token()));
}

TEST_F(ActiveDevicesProviderImplTest, ShouldReturnEmptyListWhenTooManyDevices) {
  // Create many devices to exceed the limit of the list.
  const size_t kActiveDevicesNumber =
      switches::kSyncFCMRegistrationTokensListMaxSize.Get() + 1;

  for (size_t i = 0; i < kActiveDevicesNumber; ++i) {
    const std::string device_name = "device_" + base::NumberToString(i);
    const std::string fcm_token = "fcm_token_" + device_name;
    AddDevice(device_name, fcm_token,
              clock_.Now() - base::TimeDelta::FromMinutes(1));
  }

  EXPECT_THAT(
      active_devices_provider_.CollectFCMRegistrationTokensForInvalidations(
          "guid"),
      IsEmpty());
}

}  // namespace
}  // namespace browser_sync
