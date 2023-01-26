// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/sync_device_info_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_util.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
namespace {

using syncer::DeviceInfo;
using OsType = syncer::DeviceInfo::OsType;
using DeviceCountByOsTypeMap = std::map<OsType, int>;
using syncer::FakeDeviceInfoTracker;
using DeviceType = sync_pb::SyncEnums::DeviceType;
using DeviceCountByOsTypeMap = std::map<DeviceInfo::OsType, int>;

const sync_pb::SyncEnums_DeviceType kLocalDeviceType =
    sync_pb::SyncEnums_DeviceType_TYPE_LINUX;
const DeviceInfo::OsType kLocalDeviceOS = DeviceInfo::OsType::kLinux;
const DeviceInfo::FormFactor kLocalDeviceFormFactor =
    DeviceInfo::FormFactor::kDesktop;

std::unique_ptr<DeviceInfo> CreateDeviceInfo(
    const std::string& guid,
    DeviceType device_type,
    OsType os_type,
    base::Time last_updated = base::Time::Now()) {
  return std::make_unique<DeviceInfo>(
      guid, "name", "chrome_version", "user_agent", device_type, os_type,
      kLocalDeviceFormFactor, "device_id", "manufacturer_name", "model_name",
      "full_hardware_class", last_updated,
      syncer::DeviceInfoUtil::GetPulseInterval(),
      /*send_tab_to_self_receiving_enabled=*/false, absl::nullopt,
      /*paask_info=*/absl::nullopt,
      /*fcm_registration_token=*/std::string(),
      /*interested_data_types=*/syncer::ModelTypeSet());
}

class SyncDeviceInfoObserverTest : public testing::Test {
 protected:
  SyncDeviceInfoObserverTest() = default;
  ~SyncDeviceInfoObserverTest() override = default;

  void SetUp() override {
    device_info_tracker_ = std::make_unique<FakeDeviceInfoTracker>();
    sync_device_info_observer_ =
        std::make_unique<SyncDeviceInfoObserver>(device_info_tracker_.get());
  }

  void TearDown() override {
    sync_device_info_observer_.reset();
    device_info_tracker_.reset();
  }

  std::unique_ptr<FakeDeviceInfoTracker> device_info_tracker_;
  std::unique_ptr<SyncDeviceInfoObserver> sync_device_info_observer_;
};

TEST_F(SyncDeviceInfoObserverTest, OnDeviceInfoChange_LocalDevice) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<DeviceInfo> local_device_info =
      CreateDeviceInfo("local_device", kLocalDeviceType, kLocalDeviceOS);
  // Adding a device triggers OnDeviceInfoChange().
  device_info_tracker_->Add(local_device_info.get());

  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Linux", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Windows", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Mac", 0, 1);
}

TEST_F(SyncDeviceInfoObserverTest, OnDeviceInfoChange_DifferentGuids) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<DeviceInfo> local_device_info =
      CreateDeviceInfo("local_device", kLocalDeviceType, kLocalDeviceOS);
  std::unique_ptr<DeviceInfo> local_device_info_2 =
      CreateDeviceInfo("local_device_2", kLocalDeviceType, kLocalDeviceOS);
  device_info_tracker_->Add(
      {local_device_info.get(), local_device_info_2.get()});

  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Linux", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Windows", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Mac", 0, 1);
}

TEST_F(SyncDeviceInfoObserverTest, OnDeviceInfoChange_DifferentOS) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<DeviceInfo> local_device_info =
      CreateDeviceInfo("local_device", kLocalDeviceType, kLocalDeviceOS);
  std::unique_ptr<DeviceInfo> local_device_info_2 =
      CreateDeviceInfo("local_device_2", kLocalDeviceType, OsType::kMac);
  device_info_tracker_->Add(
      {local_device_info.get(), local_device_info_2.get()});
  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Linux", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Mac", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Windows", 0, 1);
}

TEST_F(SyncDeviceInfoObserverTest, OnDeviceInfoChange_InactiveDevice) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<DeviceInfo> local_device_info =
      CreateDeviceInfo("local_device", kLocalDeviceType, kLocalDeviceOS);
  std::unique_ptr<DeviceInfo> local_device_info_2 =
      CreateDeviceInfo("local_device_2", kLocalDeviceType, OsType::kMac,
                       base::Time::Now() - base::Days(20));
  device_info_tracker_->Add(
      {local_device_info.get(), local_device_info_2.get()});

  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Linux", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Mac", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "SegmentationPlatform.DeviceCountByOsType.Windows", 0, 1);
}

}  // namespace

}  // namespace segmentation_platform
