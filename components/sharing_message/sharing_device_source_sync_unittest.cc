// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_device_source_sync.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/send_tab_to_self/features.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_name_util.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_device_info/fake_local_device_info_provider.h"
#include "components/sync_device_info/local_device_info_util.h"
#include "components/sync_device_info/test_device_info_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using SharingFeature = syncer::DeviceInfo::SharingFeature;

const char kSenderIdFCMToken[] = "sharing_fcm_token";
const char kSenderIdP256dh[] = "sharing_p256dh";
const char kSenderIdAuthSecret[] = "sharing_auth_secret";
const char kChimeRepresentativeTargetId[] = "chime_rep_id";

std::unique_ptr<syncer::DeviceInfo> CreateDeviceInfo(
    const std::string& client_name,
    SharingFeature enabled_feature,
    const std::string& manufacturer_name = "manufacturer",
    const std::string& model_name = "model",
    syncer::DeviceInfo::SharingTargetInfo sender_id_target_info =
        {kSenderIdFCMToken, kSenderIdP256dh, kSenderIdAuthSecret},
    const std::string& chime_rep_id = kChimeRepresentativeTargetId) {
  return syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kLinux)
      .WithGuid(base::Uuid::GenerateRandomV4().AsLowercaseString())
      .WithClientName(client_name)
      .WithSharingInfo(
          {{std::move(sender_id_target_info), chime_rep_id, {enabled_feature}}})
      .WithManufacturerName(manufacturer_name)
      .WithModelName(model_name)
      .Build();
}

class SharingDeviceSourceSyncTest : public testing::Test {
 public:
  std::unique_ptr<SharingDeviceSourceSync> CreateDeviceSource(
      bool wait_until_ready) {
    auto device_source = std::make_unique<SharingDeviceSourceSync>(
        &test_sync_service_, &fake_local_device_info_provider_,
        &fake_device_info_tracker_);
    if (!wait_until_ready) {
      return device_source;
    }

    if (!fake_device_info_tracker_.IsSyncing()) {
      fake_device_info_tracker_.Add(local_device_info_);
    }
    fake_local_device_info_provider_.SetReady(true);

    // Wait until local personalizable device
    base::RunLoop run_loop;
    device_source->AddReadyCallback(run_loop.QuitClosure());
    run_loop.Run();

    return device_source;
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;

  syncer::TestSyncService test_sync_service_;
  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service_;
  syncer::FakeLocalDeviceInfoProvider fake_local_device_info_provider_;
  syncer::FakeDeviceInfoTracker fake_device_info_tracker_;
  raw_ptr<const syncer::DeviceInfo> local_device_info_ =
      fake_local_device_info_provider_.GetLocalDeviceInfo();
};

}  // namespace

TEST_F(SharingDeviceSourceSyncTest, RunsReadyCallback) {
  fake_local_device_info_provider_.SetReady(false);
  EXPECT_FALSE(fake_device_info_tracker_.IsSyncing());
  EXPECT_FALSE(fake_local_device_info_provider_.GetLocalDeviceInfo());

  auto device_source = CreateDeviceSource(/*wait_until_ready=*/false);

  bool did_run_callback = false;
  device_source->AddReadyCallback(base::BindLambdaForTesting(
      [&did_run_callback]() { did_run_callback = true; }));
  EXPECT_FALSE(did_run_callback);

  // Make DeviceInfoTracker ready.
  fake_device_info_tracker_.Add(local_device_info_);
  EXPECT_FALSE(did_run_callback);

  // Set LocalDeviceInfoProvider ready.
  fake_local_device_info_provider_.SetReady(true);
  EXPECT_TRUE(did_run_callback);
}

TEST_F(SharingDeviceSourceSyncTest, GetDeviceByGuid_Ready) {
  auto device_source = CreateDeviceSource(/*wait_until_ready=*/true);
  EXPECT_TRUE(device_source->GetDeviceByGuid(local_device_info_->guid()));
}

TEST_F(SharingDeviceSourceSyncTest, GetDeviceByGuid_NotReady) {
  auto device_source = CreateDeviceSource(/*wait_until_ready=*/false);
  fake_device_info_tracker_.Add(local_device_info_);
  // Even if local device is not ready, querying devices should succeed.
  EXPECT_TRUE(device_source->GetDeviceByGuid(local_device_info_->guid()));
}

TEST_F(SharingDeviceSourceSyncTest, GetDeviceByGuid_UnknownGuid) {
  auto device_source = CreateDeviceSource(/*wait_until_ready=*/true);
  EXPECT_FALSE(device_source->GetDeviceByGuid("unknown"));
}

TEST_F(SharingDeviceSourceSyncTest, GetDeviceByGuid_SyncDisabled) {
  auto device_source = CreateDeviceSource(/*wait_until_ready=*/true);
  test_sync_service_.SetSignedOut();
  EXPECT_FALSE(device_source->GetDeviceByGuid(local_device_info_->guid()));
}

TEST_F(SharingDeviceSourceSyncTest, GetDeviceCandidates_Ready) {
  auto device_source = CreateDeviceSource(/*wait_until_ready=*/true);
  auto device_info =
      CreateDeviceInfo("client_name", SharingFeature::kRemoteCopy);
  fake_device_info_tracker_.Add(device_info.get());
  auto devices =
      device_source->GetDeviceCandidates(SharingFeature::kRemoteCopy);
  ASSERT_EQ(1u, devices.size());
  EXPECT_EQ(device_info->guid(), devices[0].guid());
}

TEST_F(SharingDeviceSourceSyncTest, GetDeviceCandidates_NotReady) {
  fake_local_device_info_provider_.SetReady(false);
  auto device_source = CreateDeviceSource(/*wait_until_ready=*/false);
  auto device_info =
      CreateDeviceInfo("client_name", SharingFeature::kRemoteCopy);
  fake_device_info_tracker_.Add(device_info.get());
  // Local device needs to be ready for deduplication.
  EXPECT_TRUE(
      device_source->GetDeviceCandidates(SharingFeature::kRemoteCopy)
          .empty());
}

enum class DeviceNamingMode {
  kLegacyWithDeduplication,
  kSimplifiedWithoutDeduplication,
};

class SharingDeviceSourceSyncNamingTest
    : public SharingDeviceSourceSyncTest,
      public testing::WithParamInterface<DeviceNamingMode> {
 public:
  SharingDeviceSourceSyncNamingTest() {
    if (GetParam() == DeviceNamingMode::kSimplifiedWithoutDeduplication) {
      scoped_feature_list_.InitAndEnableFeature(
          syncer::kSyncSimplifyDeviceNaming);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          syncer::kSyncSimplifyDeviceNaming);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests the deduplication and local device filtering behavior of
// GetDeviceCandidates. Parameterized by whether simplified device naming is
// enabled.
TEST_P(SharingDeviceSourceSyncNamingTest,
       GetDeviceCandidates_DeduplicationBehavior) {
  auto device_source = CreateDeviceSource(/*wait_until_ready=*/true);

  // Add two devices with the same |client_name| without hardware info.
  task_environment_.FastForwardBy(base::Seconds(10));
  auto device_info_1 =
      CreateDeviceInfo("client_name_1", SharingFeature::kRemoteCopy);
  fake_device_info_tracker_.Add(device_info_1.get());
  task_environment_.FastForwardBy(base::Seconds(10));
  auto device_info_2 =
      CreateDeviceInfo("client_name_1", SharingFeature::kRemoteCopy);
  fake_device_info_tracker_.Add(device_info_2.get());

  // Add two devices with the same hardware info.
  task_environment_.FastForwardBy(base::Seconds(10));
  auto device_info_3 =
      CreateDeviceInfo("model 1", SharingFeature::kRemoteCopy,
                       "manufacturer 1", "model 1");
  fake_device_info_tracker_.Add(device_info_3.get());
  task_environment_.FastForwardBy(base::Seconds(10));
  auto device_info_4 =
      CreateDeviceInfo("model 1", SharingFeature::kRemoteCopy,
                       "manufacturer 1", "model 1");
  fake_device_info_tracker_.Add(device_info_4.get());

  // Add a device with the same info as the local device (but different GUID).
  task_environment_.FastForwardBy(base::Seconds(10));
  auto device_info_5 = CreateDeviceInfo(local_device_info_->client_name(),
                                        SharingFeature::kRemoteCopy,
                                        local_device_info_->manufacturer_name(),
                                        local_device_info_->model_name());
  fake_device_info_tracker_.Add(device_info_5.get());

  auto devices =
      device_source->GetDeviceCandidates(SharingFeature::kRemoteCopy);

  if (GetParam() == DeviceNamingMode::kSimplifiedWithoutDeduplication) {
    // With kSyncSimplifyDeviceNaming enabled:
    // 1. No deduplication (device_info_1 and device_info_3 are kept).
    // 2. No filtering of same-name-as-local-device (device_info_5 is kept).
    // 3. Local device itself is still filtered out (by GUID).
    ASSERT_EQ(5u, devices.size());
    EXPECT_EQ(device_info_5->guid(), devices[0].guid());
    EXPECT_EQ(device_info_4->guid(), devices[1].guid());
    EXPECT_EQ(device_info_3->guid(), devices[2].guid());
    EXPECT_EQ(device_info_2->guid(), devices[3].guid());
    EXPECT_EQ(device_info_1->guid(), devices[4].guid());
  } else {
    // With kSyncSimplifyDeviceNaming disabled:
    // 1. Deduplication active (device_info_4 keeps newer, drops 3.
    // device_info_2 keeps newer, drops 1).
    // 2. Local device name matching active (device_info_5 is dropped because it
    // matches local device name).
    ASSERT_EQ(2u, devices.size());
    EXPECT_EQ(device_info_4->guid(), devices[0].guid());
    EXPECT_EQ(device_info_2->guid(), devices[1].guid());
  }
}

// Tests that device names are resolved correctly, including collision
// resolution in legacy mode and preferred names in simplified mode.
TEST_P(SharingDeviceSourceSyncNamingTest, GetDeviceCandidates_DeviceNaming) {
  auto device_source = CreateDeviceSource(/*wait_until_ready=*/true);

  task_environment_.FastForwardBy(base::Seconds(10));
  auto device_info_1 =
      CreateDeviceInfo("client_name", SharingFeature::kRemoteCopy);
  fake_device_info_tracker_.Add(device_info_1.get());

  task_environment_.FastForwardBy(base::Seconds(10));
  auto device_info_2 =
      CreateDeviceInfo("model 1", SharingFeature::kRemoteCopy,
                       "manufacturer 1", "model 1");
  fake_device_info_tracker_.Add(device_info_2.get());

  task_environment_.FastForwardBy(base::Seconds(10));
  auto device_info_3 =
      CreateDeviceInfo("model 2", SharingFeature::kRemoteCopy,
                       "manufacturer 1", "model 2");
  fake_device_info_tracker_.Add(device_info_3.get());

  task_environment_.FastForwardBy(base::Seconds(10));
  auto device_info_4 =
      CreateDeviceInfo("model 1", SharingFeature::kRemoteCopy,
                       "manufacturer 2", "model 1");
  fake_device_info_tracker_.Add(device_info_4.get());

  auto devices =
      device_source->GetDeviceCandidates(SharingFeature::kRemoteCopy);
  ASSERT_EQ(4u, devices.size());

  if (GetParam() == DeviceNamingMode::kSimplifiedWithoutDeduplication) {
    // With kSyncSimplifyDeviceNaming enabled:
    // All devices get their preferred name, even if they collide.
    EXPECT_EQ(syncer::GetDisplayNameCandidates(device_info_4.get())
                  .preferred_name_if_unique,
              devices[0].client_name());
    EXPECT_EQ(syncer::GetDisplayNameCandidates(device_info_3.get())
                  .preferred_name_if_unique,
              devices[1].client_name());
    EXPECT_EQ(syncer::GetDisplayNameCandidates(device_info_2.get())
                  .preferred_name_if_unique,
              devices[2].client_name());
    EXPECT_EQ(syncer::GetDisplayNameCandidates(device_info_1.get())
                  .preferred_name_if_unique,
              devices[3].client_name());
  } else {
    // With kSyncSimplifyDeviceNaming disabled:
    // Collision resolution is active, so colliding devices get fallback full
    // names.
    EXPECT_EQ(syncer::GetDisplayNameCandidates(device_info_4.get())
                  .preferred_name_if_unique,
              devices[0].client_name());
    EXPECT_EQ(syncer::GetDisplayNameCandidates(device_info_3.get())
                  .fallback_full_name,
              devices[1].client_name());
    EXPECT_EQ(syncer::GetDisplayNameCandidates(device_info_2.get())
                  .fallback_full_name,
              devices[2].client_name());
    EXPECT_EQ(syncer::GetDisplayNameCandidates(device_info_1.get())
                  .preferred_name_if_unique,
              devices[3].client_name());
  }
}

TEST_F(SharingDeviceSourceSyncTest, GetDeviceCandidates_Expired) {
  // Create device in advance so time can be forwarded before calling
  // GetDeviceCandidates.
  auto device_source = CreateDeviceSource(/*wait_until_ready=*/true);
  auto device_info =
      CreateDeviceInfo("model 1", SharingFeature::kRemoteCopy,
                       "manufacturer 2", "model 1");
  fake_device_info_tracker_.Add(device_info.get());

  // Forward time until device expires.
  task_environment_.FastForwardBy(kSharingDeviceExpiration +
                                  base::Milliseconds(1));

  std::vector<SharingTargetDeviceInfo> candidates =
      device_source->GetDeviceCandidates(SharingFeature::kRemoteCopy);

  EXPECT_TRUE(candidates.empty());
}

TEST_F(SharingDeviceSourceSyncTest, GetDeviceCandidates_MissingRequirements) {
  auto device_source = CreateDeviceSource(/*wait_until_ready=*/true);
  // Create device with SMS fetcher feature.
  auto device_info =
      CreateDeviceInfo("model 1", SharingFeature::kSmsFetcher,
                       "manufacturer 2", "model 1");
  fake_device_info_tracker_.Add(device_info.get());

  // Requires remote copy feature.
  std::vector<SharingTargetDeviceInfo> candidates =
      device_source->GetDeviceCandidates(SharingFeature::kRemoteCopy);

  EXPECT_TRUE(candidates.empty());
}

TEST_F(SharingDeviceSourceSyncTest,
       GetDeviceCandidates_ExpiredAndMissingRequirements) {
  auto device_source = CreateDeviceSource(/*wait_until_ready=*/true);

  // This device will be filtered out because its older than |min_updated_time|.
  task_environment_.FastForwardBy(base::Seconds(10));
  auto device_info_1 =
      CreateDeviceInfo("model 3", SharingFeature::kRemoteCopy,
                       "manufacturer 2", "model 3");
  fake_device_info_tracker_.Add(device_info_1.get());

  // This device will be displayed with its short name.
  task_environment_.FastForwardBy(kSharingDeviceExpiration);
  auto device_info_2 =
      CreateDeviceInfo("model 1", SharingFeature::kRemoteCopy,
                       "manufacturer 1", "model 1");
  fake_device_info_tracker_.Add(device_info_2.get());

  // This device will be filtered out since remote copy is not enabled.
  task_environment_.FastForwardBy(base::Seconds(10));
  auto device_info_3 = CreateDeviceInfo("model 1", SharingFeature::kSmsFetcher,
                                        "manufacturer 1", "model 1");
  fake_device_info_tracker_.Add(device_info_3.get());

  // This device will be displayed with its short name.
  task_environment_.FastForwardBy(base::Seconds(10));
  auto device_info_4 =
      CreateDeviceInfo("model 2", SharingFeature::kRemoteCopy,
                       "manufacturer 2", "model 2");
  fake_device_info_tracker_.Add(device_info_4.get());

  auto devices =
      device_source->GetDeviceCandidates(SharingFeature::kRemoteCopy);
  ASSERT_EQ(2u, devices.size());
  EXPECT_EQ(device_info_4->guid(), devices[0].guid());
  EXPECT_EQ(
      syncer::GetDisplayNameCandidates(device_info_4.get()).preferred_name_if_unique,
      devices[0].client_name());
  EXPECT_EQ(device_info_2->guid(), devices[1].guid());
  EXPECT_EQ(
      syncer::GetDisplayNameCandidates(device_info_2.get()).preferred_name_if_unique,
      devices[1].client_name());
}

TEST_F(SharingDeviceSourceSyncTest, GetDeviceCandidates_NoChannel) {
  auto device_source = CreateDeviceSource(/*wait_until_ready=*/true);
  auto device_info =
      CreateDeviceInfo("client_name", SharingFeature::kRemoteCopy,
                       "manufacturer", "model",
                       /*sender_id_target_info=*/{});
  fake_device_info_tracker_.Add(device_info.get());

  auto devices =
      device_source->GetDeviceCandidates(SharingFeature::kRemoteCopy);
  EXPECT_TRUE(devices.empty());
}

TEST_F(SharingDeviceSourceSyncTest, GetDeviceCandidates_FCMChannel) {
  auto device_source = CreateDeviceSource(/*wait_until_ready=*/true);
  auto device_info =
      CreateDeviceInfo("client_name", SharingFeature::kRemoteCopy,
                       "manufacturer", "model",
                       /*sender_id_target_info=*/{});
  fake_device_info_tracker_.Add(device_info.get());

  auto devices =
      device_source->GetDeviceCandidates(SharingFeature::kRemoteCopy);

  // FCM channel is not supported.
  ASSERT_EQ(0u, devices.size());
}

TEST_F(SharingDeviceSourceSyncTest, GetDeviceCandidates_SenderIDChannel) {
  auto device_source = CreateDeviceSource(/*wait_until_ready=*/true);
  auto device_info = CreateDeviceInfo(
      "client_name", SharingFeature::kRemoteCopy, "manufacturer",
      "model", {kSenderIdFCMToken, kSenderIdP256dh, kSenderIdAuthSecret});
  fake_device_info_tracker_.Add(device_info.get());

  auto devices =
      device_source->GetDeviceCandidates(SharingFeature::kRemoteCopy);
  ASSERT_EQ(1u, devices.size());
  EXPECT_EQ(device_info->guid(), devices[0].guid());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SharingDeviceSourceSyncNamingTest,
    testing::Values(DeviceNamingMode::kLegacyWithDeduplication,
                    DeviceNamingMode::kSimplifiedWithoutDeduplication));
