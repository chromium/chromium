// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/active_devices_provider_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/active_devices_invalidation_info.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::ActiveDevicesInvalidationInfo;
using syncer::DataTypeSet;
using syncer::DeviceInfo;
using syncer::FakeDeviceInfoTracker;
using testing::Contains;
using testing::IsEmpty;
using testing::SizeIs;
using testing::UnorderedElementsAre;

namespace browser_sync {
namespace {

constexpr int kPulseIntervalMinutes = 60;

std::unique_ptr<DeviceInfo> CreateFakeDeviceInfo(
    const std::string& name,
    const std::string& fcm_registration_token,
    const DataTypeSet& interested_data_types,
    base::Time last_updated_timestamp) {
  return std::make_unique<syncer::DeviceInfo>(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), name,
      "chrome_version", "user_agent", sync_pb::SyncEnums::TYPE_UNSET,
      syncer::DeviceInfo::OsType::kUnknown,
      syncer::DeviceInfo::FormFactor::kUnknown, "device_id",
      "manufacturer_name", "model_name", "full_hardware_class",
      last_updated_timestamp, base::Minutes(kPulseIntervalMinutes),
      /*send_tab_to_self_receiving_enabled=*/
      false,
      /*send_tab_to_self_receiving_type=*/
      sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
      /*sharing_info=*/std::nullopt, /*paask_info=*/std::nullopt,
      fcm_registration_token, interested_data_types,
      /*floating_workspace_last_signin_timestamp=*/std::nullopt);
}

DataTypeSet DefaultInterestedDataTypes() {
  return Difference(syncer::ProtocolTypes(), syncer::CommitOnlyTypes());
}

class ActiveDevicesProviderImplTest : public testing::Test {
 public:
  ActiveDevicesProviderImplTest()
      : active_devices_provider_(&fake_device_info_tracker_, &clock_) {}

  ~ActiveDevicesProviderImplTest() override = default;

  void AddDevice(const std::string& name,
                 const std::string& fcm_registration_token,
                 const DataTypeSet& interested_data_types,
                 base::Time last_updated_timestamp) {
    device_list_.push_back(CreateFakeDeviceInfo(name, fcm_registration_token,
                                                interested_data_types,
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
  base::test::ScopedFeatureList feature_override(
      switches::kSyncFilterOutInactiveDevicesForSingleClient);
  AddDevice("local_device_pulse_interval",
            /*fcm_registration_token=*/"token_1", DefaultInterestedDataTypes(),
            clock_.Now() - base::Minutes(kPulseIntervalMinutes + 1));

  // Very old device.
  AddDevice("device_inactive", /*fcm_registration_token=*/"token_2",
            DefaultInterestedDataTypes(), clock_.Now() - base::Days(100));

  // The local device should be considered active due to margin even though the
  // device is outside the pulse interval. This is not a single client because
  // the device waits to receive self-invalidations (because it has not
  // specified a |local_cache_guid|).
  const ActiveDevicesInvalidationInfo result_no_guid =
      active_devices_provider_.CalculateInvalidationInfo(
          /*local_cache_guid=*/std::string());
  EXPECT_FALSE(result_no_guid.IsSingleClientForTypes({syncer::BOOKMARKS}));
  EXPECT_THAT(result_no_guid.all_fcm_registration_tokens(),
              Contains("token_1"));
  EXPECT_FALSE(result_no_guid.IsSingleClientWithStandaloneInvalidationsForTypes(
      {syncer::BOOKMARKS}));
  EXPECT_THAT(result_no_guid.GetFcmRegistrationTokensForInterestedClients(
                  {syncer::BOOKMARKS}),
              Contains("token_1"));

  // Should ignore the local device and ignore the old device even if it's
  // interested in bookmarks.
  ASSERT_THAT(device_list_, SizeIs(2));
  const ActiveDevicesInvalidationInfo result_local_guid =
      active_devices_provider_.CalculateInvalidationInfo(
          device_list_.front()->guid());
  EXPECT_TRUE(result_local_guid.IsSingleClientForTypes({syncer::BOOKMARKS}));
  EXPECT_TRUE(
      result_local_guid.IsSingleClientWithStandaloneInvalidationsForTypes(
          {syncer::BOOKMARKS}));
}

TEST_F(ActiveDevicesProviderImplTest, ShouldReturnIfSingleDeviceByDataType) {
  AddDevice("local_device", /*fcm_registration_token=*/"",
            DefaultInterestedDataTypes(), clock_.Now());
  AddDevice("remote_device", /*fcm_registration_token=*/"",
            Difference(DefaultInterestedDataTypes(), {syncer::SESSIONS}),
            clock_.Now());

  // Remote device has disabled sessions data type and current device should be
  // considered as the only client.
  const ActiveDevicesInvalidationInfo result_local_guid =
      active_devices_provider_.CalculateInvalidationInfo(
          device_list_.front()->guid());
  EXPECT_TRUE(result_local_guid.IsSingleClientForTypes({syncer::SESSIONS}));
  EXPECT_FALSE(result_local_guid.IsSingleClientForTypes({syncer::BOOKMARKS}));

  // Standalone invalidations are disabled, hence it should be a single device
  // for BOOKMARKS as well.
  EXPECT_TRUE(
      result_local_guid.IsSingleClientWithStandaloneInvalidationsForTypes(
          {syncer::BOOKMARKS}));
}

TEST_F(ActiveDevicesProviderImplTest,
       ShouldReturnIfSingleClientWithStandaloneInvalidations) {
  AddDevice("local_device", /*fcm_registration_token=*/"token_1",
            DefaultInterestedDataTypes(), clock_.Now());
  AddDevice("remote_device", /*fcm_registration_token=*/"token_2",
            Difference(DefaultInterestedDataTypes(), {syncer::SESSIONS}),
            clock_.Now());

  // Remote device has disabled sessions data type and current device should be
  // considered as the only client.
  const ActiveDevicesInvalidationInfo result_local_guid =
      active_devices_provider_.CalculateInvalidationInfo(
          device_list_.front()->guid());
  EXPECT_TRUE(
      result_local_guid.IsSingleClientWithStandaloneInvalidationsForTypes(
          {syncer::SESSIONS}));
  EXPECT_FALSE(
      result_local_guid.IsSingleClientWithStandaloneInvalidationsForTypes(
          {syncer::BOOKMARKS}));

  // Since all the clients with enabled standalone invalidations, single client
  // should have the same results.
  EXPECT_TRUE(result_local_guid.IsSingleClientForTypes({syncer::SESSIONS}));
  EXPECT_FALSE(result_local_guid.IsSingleClientForTypes({syncer::BOOKMARKS}));
}

TEST_F(ActiveDevicesProviderImplTest,
       ShouldReturnSingleClientForStandaloneInvalidationsOnly) {
  AddDevice("local_device", /*fcm_registration_token=*/"token_1",
            DefaultInterestedDataTypes(), clock_.Now());

  // A remote device doesn't subscribe to standalone invalidations.
  AddDevice("remote_device", /*fcm_registration_token=*/"",
            DefaultInterestedDataTypes(), clock_.Now());

  const ActiveDevicesInvalidationInfo result =
      active_devices_provider_.CalculateInvalidationInfo(
          device_list_.front()->guid());

  EXPECT_FALSE(result.IsSingleClientForTypes({syncer::BOOKMARKS}));
  EXPECT_TRUE(result.IsSingleClientWithStandaloneInvalidationsForTypes(
      {syncer::BOOKMARKS}));
}

TEST_F(ActiveDevicesProviderImplTest,
       ShouldReturnSingleClientForOldInvalidations) {
  // Add only devices with standalone invalidations.
  AddDevice("local_device", /*fcm_registration_token=*/"token_1",
            DefaultInterestedDataTypes(), clock_.Now());
  AddDevice("remote_device", /*fcm_registration_token=*/"token_2",
            DefaultInterestedDataTypes(), clock_.Now());

  EXPECT_TRUE(
      active_devices_provider_
          .CalculateInvalidationInfo(device_list_.front()->guid())
          .IsSingleClientWithOldInvalidationsForTypes({syncer::BOOKMARKS}));

  // Add a remote device subscribed to old invalidatoins.
  AddDevice("remote_deivce_2", /*fcm_registration_token=*/"",
            DefaultInterestedDataTypes(), clock_.Now());

  EXPECT_FALSE(
      active_devices_provider_
          .CalculateInvalidationInfo(device_list_.front()->guid())
          .IsSingleClientWithOldInvalidationsForTypes({syncer::BOOKMARKS}));
}

TEST_F(ActiveDevicesProviderImplTest,
       ShouldReturnSingleClientForOldInvalidationsForInterestedDataTypes) {
  AddDevice("local_device", /*fcm_registration_token=*/"token_1",
            DefaultInterestedDataTypes(), clock_.Now());

  // Add a remote device which is not interested in SESSIONS.
  AddDevice("remote_device", /*fcm_registration_token=*/"",
            Difference(DefaultInterestedDataTypes(), {syncer::SESSIONS}),
            clock_.Now());

  EXPECT_FALSE(
      active_devices_provider_
          .CalculateInvalidationInfo(device_list_.front()->guid())
          .IsSingleClientWithOldInvalidationsForTypes({syncer::BOOKMARKS}));
  EXPECT_TRUE(
      active_devices_provider_
          .CalculateInvalidationInfo(device_list_.front()->guid())
          .IsSingleClientWithOldInvalidationsForTypes({syncer::SESSIONS}));

  // Add a remote device which does not support interested data types.
  AddDevice("old_remote_device", /*fcm_registration_token=*/"",
            /*interested_data_types=*/{}, clock_.Now());

  EXPECT_FALSE(
      active_devices_provider_
          .CalculateInvalidationInfo(device_list_.front()->guid())
          .IsSingleClientWithOldInvalidationsForTypes({syncer::BOOKMARKS}));
  EXPECT_FALSE(
      active_devices_provider_
          .CalculateInvalidationInfo(device_list_.front()->guid())
          .IsSingleClientWithOldInvalidationsForTypes({syncer::SESSIONS}));
}

TEST_F(ActiveDevicesProviderImplTest, ShouldReturnZeroDevices) {
  const ActiveDevicesInvalidationInfo result =
      active_devices_provider_.CalculateInvalidationInfo(
          /*local_cache_guid=*/std::string());

  // If there are no devices at all (including the local device), that means we
  // just don't have the device information yet, so we should *not* consider
  // this a single-client situation.
  EXPECT_THAT(result.all_fcm_registration_tokens(), IsEmpty());
  EXPECT_FALSE(result.IsSingleClientForTypes({syncer::BOOKMARKS}));
  EXPECT_FALSE(result.IsSingleClientWithStandaloneInvalidationsForTypes(
      {syncer::BOOKMARKS}));
  EXPECT_FALSE(
      result.IsSingleClientWithOldInvalidationsForTypes({syncer::BOOKMARKS}));
  EXPECT_THAT(
      result.GetFcmRegistrationTokensForInterestedClients({syncer::BOOKMARKS}),
      IsEmpty());
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
  base::test::ScopedFeatureList feature_override(
      switches::kSyncFilterOutInactiveDevicesForSingleClient);
  AddDevice("device_1", "fcm_token_1", DefaultInterestedDataTypes(),
            clock_.Now() - base::Minutes(1));
  AddDevice("device_2", "fcm_token_2", DefaultInterestedDataTypes(),
            clock_.Now() - base::Minutes(1));
  AddDevice("device_inactive", "fcm_token_3", DefaultInterestedDataTypes(),
            clock_.Now() - base::Days(100));

  ASSERT_EQ(3u, device_list_.size());

  const ActiveDevicesInvalidationInfo result_no_guid =
      active_devices_provider_.CalculateInvalidationInfo(
          /*local_cache_guid=*/std::string());
  EXPECT_THAT(result_no_guid.all_fcm_registration_tokens(),
              UnorderedElementsAre(device_list_[0]->fcm_registration_token(),
                                   device_list_[1]->fcm_registration_token()));

  const ActiveDevicesInvalidationInfo result_local_guid =
      active_devices_provider_.CalculateInvalidationInfo(
          device_list_[0]->guid());
  EXPECT_THAT(result_local_guid.all_fcm_registration_tokens(),
              UnorderedElementsAre(device_list_[1]->fcm_registration_token()));
  EXPECT_THAT(result_local_guid.GetFcmRegistrationTokensForInterestedClients(
                  {syncer::BOOKMARKS}),
              UnorderedElementsAre(device_list_[1]->fcm_registration_token()));
}

TEST_F(ActiveDevicesProviderImplTest, ShouldReturnEmptyListWhenTooManyDevices) {
  // Create many devices to exceed the limit of the list.
  const size_t kActiveDevicesNumber =
      switches::kSyncFCMRegistrationTokensListMaxSize.Get() + 1;

  for (size_t i = 0; i < kActiveDevicesNumber; ++i) {
    const std::string device_name = "device_" + base::NumberToString(i);
    const std::string fcm_token = "fcm_token_" + device_name;
    AddDevice(device_name, fcm_token, DefaultInterestedDataTypes(),
              clock_.Now() - base::Minutes(1));
  }

  ActiveDevicesInvalidationInfo result =
      active_devices_provider_.CalculateInvalidationInfo(
          /*local_cache_guid=*/std::string());
  EXPECT_THAT(result.all_fcm_registration_tokens(), IsEmpty());
  EXPECT_FALSE(result.IsSingleClientWithStandaloneInvalidationsForTypes(
      {syncer::BOOKMARKS}));
  EXPECT_THAT(
      result.GetFcmRegistrationTokensForInterestedClients({syncer::BOOKMARKS}),
      IsEmpty());

  // Double check that all other devices will result in an empty FCM
  // registration token list.
  AddDevice("extra_device", "extra_token", DefaultInterestedDataTypes(),
            clock_.Now());
  result = active_devices_provider_.CalculateInvalidationInfo(
      /*local_cache_guid=*/std::string());
  EXPECT_THAT(result.all_fcm_registration_tokens(), IsEmpty());
  EXPECT_FALSE(result.IsSingleClientWithStandaloneInvalidationsForTypes(
      {syncer::BOOKMARKS}));
  EXPECT_THAT(
      result.GetFcmRegistrationTokensForInterestedClients({syncer::BOOKMARKS}),
      IsEmpty());
}

TEST_F(ActiveDevicesProviderImplTest,
       ShouldReturnFCMRegistrationTokensFromLatestDeviceInfo) {
  AddDevice("device_1", "fcm_token", DefaultInterestedDataTypes(),
            clock_.Now() - base::Minutes(2));
  // A newer device with the same FCM registration token but different
  // interested data types.
  AddDevice("device_2", "fcm_token",
            Difference(DefaultInterestedDataTypes(), {syncer::BOOKMARKS}),
            clock_.Now() - base::Minutes(1));

  ASSERT_EQ(2u, device_list_.size());

  const ActiveDevicesInvalidationInfo result_no_guid =
      active_devices_provider_.CalculateInvalidationInfo(
          /*local_cache_guid=*/std::string());
  EXPECT_THAT(result_no_guid.GetFcmRegistrationTokensForInterestedClients(
                  {syncer::SESSIONS}),
              UnorderedElementsAre(device_list_[1]->fcm_registration_token()));
  EXPECT_THAT(result_no_guid.GetFcmRegistrationTokensForInterestedClients(
                  {syncer::BOOKMARKS}),
              IsEmpty());
}

}  // namespace
}  // namespace browser_sync
