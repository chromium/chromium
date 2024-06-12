// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/tab_group_sync_metrics_logger.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "components/saved_tab_groups/saved_tab_group_test_utils.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::Return;

namespace tab_groups {
namespace {

const char kDeviceGuid1[] = "device1";
const char kDeviceGuid2[] = "device2";

}  // namespace

class TabGroupSyncMetricsLoggerTest : public testing::Test {
 public:
  TabGroupSyncMetricsLoggerTest() = default;

  ~TabGroupSyncMetricsLoggerTest() override = default;

  void SetUp() override {
    device_info_tracker_ = std::make_unique<syncer::FakeDeviceInfoTracker>();
    metrics_logger_ =
        std::make_unique<TabGroupSyncMetricsLogger>(device_info_tracker_.get());
  }

  DeviceType GetDeviceType(syncer::DeviceInfo::OsType os_type,
                           syncer::DeviceInfo::FormFactor form_factor) {
    auto device_info = test::CreateDeviceInfo("test", os_type, form_factor);
    return metrics_logger_->GetDeviceTypeFromDeviceInfo(*device_info);
  }

 protected:
  std::unique_ptr<syncer::FakeDeviceInfoTracker> device_info_tracker_;
  std::unique_ptr<TabGroupSyncMetricsLogger> metrics_logger_;
};

TEST_F(TabGroupSyncMetricsLoggerTest, HistogramsAreEmittedForLogEvents) {
  base::HistogramTester histogram_tester;
  auto device_info1 =
      test::CreateDeviceInfo(kDeviceGuid1, syncer::DeviceInfo::OsType::kAndroid,
                             syncer::DeviceInfo::FormFactor::kPhone);
  auto device_info2 =
      test::CreateDeviceInfo(kDeviceGuid2, syncer::DeviceInfo::OsType::kWindows,
                             syncer::DeviceInfo::FormFactor::kDesktop);
  device_info_tracker_->Add(device_info1.get());
  device_info_tracker_->Add(device_info2.get());

  // Group events.
  metrics_logger_->LogEvent(TabGroupEvent::kTabGroupCreated, kDeviceGuid1,
                            std::nullopt);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.TabGroup.Created.GroupCreateOrigin",
      DeviceType::kAndroidPhone, 1u);

  metrics_logger_->LogEvent(TabGroupEvent::kTabGroupRemoved, kDeviceGuid1,
                            std::nullopt);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.TabGroup.Removed.GroupCreateOrigin",
      DeviceType::kAndroidPhone, 1u);

  metrics_logger_->LogEvent(TabGroupEvent::kTabGroupOpened, kDeviceGuid1,
                            std::nullopt);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.TabGroup.Opened.GroupCreateOrigin",
      DeviceType::kAndroidPhone, 1u);

  metrics_logger_->LogEvent(TabGroupEvent::kTabGroupClosed, kDeviceGuid1,
                            std::nullopt);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.TabGroup.Closed.GroupCreateOrigin",
      DeviceType::kAndroidPhone, 1u);

  metrics_logger_->LogEvent(TabGroupEvent::kTabGroupVisualsChanged,
                            kDeviceGuid1, std::nullopt);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.TabGroup.VisualsChanged.GroupCreateOrigin",
      DeviceType::kAndroidPhone, 1u);

  metrics_logger_->LogEvent(TabGroupEvent::kTabGroupTabsReordered, kDeviceGuid1,
                            std::nullopt);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.TabGroup.TabsReordered.GroupCreateOrigin",
      DeviceType::kAndroidPhone, 1u);

  // Tab events.
  metrics_logger_->LogEvent(TabGroupEvent::kTabAdded, kDeviceGuid1,
                            std::nullopt);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.TabGroup.TabAdded.GroupCreateOrigin",
      DeviceType::kAndroidPhone, 1u);

  metrics_logger_->LogEvent(TabGroupEvent::kTabRemoved, kDeviceGuid1,
                            kDeviceGuid2);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.TabGroup.TabRemoved.GroupCreateOrigin",
      DeviceType::kAndroidPhone, 1u);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.TabGroup.TabRemoved.TabCreateOrigin",
      DeviceType::kWindows, 1u);

  metrics_logger_->LogEvent(TabGroupEvent::kTabNavigated, kDeviceGuid1,
                            kDeviceGuid2);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.TabGroup.TabNavigated.GroupCreateOrigin",
      DeviceType::kAndroidPhone, 1u);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.TabGroup.TabNavigated.TabCreateOrigin",
      DeviceType::kWindows, 1u);
}

TEST_F(TabGroupSyncMetricsLoggerTest, DeviceTypeConversion) {
  EXPECT_EQ(DeviceType::kAndroidPhone,
            GetDeviceType(syncer::DeviceInfo::OsType::kAndroid,
                          syncer::DeviceInfo::FormFactor::kPhone));
  EXPECT_EQ(DeviceType::kAndroidTablet,
            GetDeviceType(syncer::DeviceInfo::OsType::kAndroid,
                          syncer::DeviceInfo::FormFactor::kTablet));
  EXPECT_EQ(DeviceType::kIOSPhone,
            GetDeviceType(syncer::DeviceInfo::OsType::kIOS,
                          syncer::DeviceInfo::FormFactor::kPhone));
  EXPECT_EQ(DeviceType::kIOSTablet,
            GetDeviceType(syncer::DeviceInfo::OsType::kIOS,
                          syncer::DeviceInfo::FormFactor::kTablet));

  // Unknown / unsupported form factors in Android / IOS.
  EXPECT_EQ(DeviceType::kUnknown,
            GetDeviceType(syncer::DeviceInfo::OsType::kAndroid,
                          syncer::DeviceInfo::FormFactor::kUnknown));
  EXPECT_EQ(DeviceType::kUnknown,
            GetDeviceType(syncer::DeviceInfo::OsType::kAndroid,
                          syncer::DeviceInfo::FormFactor::kDesktop));
  EXPECT_EQ(DeviceType::kUnknown,
            GetDeviceType(syncer::DeviceInfo::OsType::kIOS,
                          syncer::DeviceInfo::FormFactor::kUnknown));
  EXPECT_EQ(DeviceType::kUnknown,
            GetDeviceType(syncer::DeviceInfo::OsType::kIOS,
                          syncer::DeviceInfo::FormFactor::kDesktop));

  // Unknown OS types.
  EXPECT_EQ(DeviceType::kUnknown,
            GetDeviceType(syncer::DeviceInfo::OsType::kUnknown,
                          syncer::DeviceInfo::FormFactor::kPhone));
  EXPECT_EQ(DeviceType::kUnknown,
            GetDeviceType(syncer::DeviceInfo::OsType::kUnknown,
                          syncer::DeviceInfo::FormFactor::kTablet));
  EXPECT_EQ(DeviceType::kUnknown,
            GetDeviceType(syncer::DeviceInfo::OsType::kUnknown,
                          syncer::DeviceInfo::FormFactor::kDesktop));
  EXPECT_EQ(DeviceType::kUnknown,
            GetDeviceType(syncer::DeviceInfo::OsType::kUnknown,
                          syncer::DeviceInfo::FormFactor::kUnknown));

  // Desktop OS types.
  EXPECT_EQ(DeviceType::kWindows,
            GetDeviceType(syncer::DeviceInfo::OsType::kWindows,
                          syncer::DeviceInfo::FormFactor::kDesktop));
  EXPECT_EQ(DeviceType::kMac,
            GetDeviceType(syncer::DeviceInfo::OsType::kMac,
                          syncer::DeviceInfo::FormFactor::kDesktop));
  EXPECT_EQ(DeviceType::kLinux,
            GetDeviceType(syncer::DeviceInfo::OsType::kLinux,
                          syncer::DeviceInfo::FormFactor::kDesktop));
  EXPECT_EQ(DeviceType::kChromeOS,
            GetDeviceType(syncer::DeviceInfo::OsType::kChromeOsAsh,
                          syncer::DeviceInfo::FormFactor::kDesktop));
  EXPECT_EQ(DeviceType::kChromeOS,
            GetDeviceType(syncer::DeviceInfo::OsType::kChromeOsLacros,
                          syncer::DeviceInfo::FormFactor::kDesktop));

  // Unsupported desktop OS form factors.
  EXPECT_EQ(DeviceType::kWindows,
            GetDeviceType(syncer::DeviceInfo::OsType::kWindows,
                          syncer::DeviceInfo::FormFactor::kPhone));
  EXPECT_EQ(DeviceType::kLinux,
            GetDeviceType(syncer::DeviceInfo::OsType::kLinux,
                          syncer::DeviceInfo::FormFactor::kTablet));
  EXPECT_EQ(DeviceType::kChromeOS,
            GetDeviceType(syncer::DeviceInfo::OsType::kChromeOsAsh,
                          syncer::DeviceInfo::FormFactor::kPhone));
  EXPECT_EQ(DeviceType::kChromeOS,
            GetDeviceType(syncer::DeviceInfo::OsType::kChromeOsLacros,
                          syncer::DeviceInfo::FormFactor::kTablet));
}

}  // namespace tab_groups
