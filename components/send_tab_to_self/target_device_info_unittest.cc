// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/target_device_info.h"

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/send_tab_to_self/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_device_info/device_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace send_tab_to_self {
namespace {

class TargetDeviceInfoWithImprovedLabelsTest : public testing::Test {
 public:
  TargetDeviceInfoWithImprovedLabelsTest() {
    feature_list_.InitAndEnableFeature(kSendTabToSelfImprovedLastActiveLabels);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  base::test::ScopedFeatureList feature_list_;
};

class TargetDeviceInfoWithImprovedLabelsDisabledTest : public testing::Test {
 public:
  TargetDeviceInfoWithImprovedLabelsDisabledTest() {
    feature_list_.InitAndDisableFeature(kSendTabToSelfImprovedLastActiveLabels);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TargetDeviceInfoWithImprovedLabelsTest, ActiveNow) {
  base::Time last_updated = base::Time::Now() - base::Seconds(30);
  TargetDeviceInfo device_info(
      "device", "guid", syncer::DeviceInfo::FormFactor::kDesktop, last_updated,
      /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_NOW),
            device_info.GetLastActiveTimeForDisplay());
}

TEST_F(TargetDeviceInfoWithImprovedLabelsTest, ActiveMinutes) {
  base::Time last_updated = base::Time::Now() - base::Minutes(5);
  TargetDeviceInfo device_info(
      "device", "guid", syncer::DeviceInfo::FormFactor::kDesktop, last_updated,
      /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_MINUTES, 5),
            device_info.GetLastActiveTimeForDisplay());
}

TEST_F(TargetDeviceInfoWithImprovedLabelsTest, ActiveHours) {
  base::Time last_updated = base::Time::Now() - base::Hours(5);
  TargetDeviceInfo device_info(
      "device", "guid", syncer::DeviceInfo::FormFactor::kDesktop, last_updated,
      /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_HOURS, 5),
            device_info.GetLastActiveTimeForDisplay());
}

TEST_F(TargetDeviceInfoWithImprovedLabelsTest, ActiveOneMinute) {
  base::Time last_updated = base::Time::Now() - base::Minutes(1);
  TargetDeviceInfo device_info(
      "device", "guid", syncer::DeviceInfo::FormFactor::kDesktop, last_updated,
      /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_MINUTES, 1),
            device_info.GetLastActiveTimeForDisplay());
}

TEST_F(TargetDeviceInfoWithImprovedLabelsTest, ActiveFiftyNineMinutes) {
  base::Time last_updated = base::Time::Now() - base::Minutes(59);
  TargetDeviceInfo device_info(
      "device", "guid", syncer::DeviceInfo::FormFactor::kDesktop, last_updated,
      /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_MINUTES, 59),
            device_info.GetLastActiveTimeForDisplay());
}

TEST_F(TargetDeviceInfoWithImprovedLabelsTest, ActiveOneHour) {
  base::Time last_updated = base::Time::Now() - base::Hours(1);
  TargetDeviceInfo device_info(
      "device", "guid", syncer::DeviceInfo::FormFactor::kDesktop, last_updated,
      /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_HOURS, 1),
            device_info.GetLastActiveTimeForDisplay());
}

TEST_F(TargetDeviceInfoWithImprovedLabelsTest, ActiveTwentyThreeHours) {
  base::Time last_updated = base::Time::Now() - base::Hours(23);
  TargetDeviceInfo device_info(
      "device", "guid", syncer::DeviceInfo::FormFactor::kDesktop, last_updated,
      /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_HOURS, 23),
            device_info.GetLastActiveTimeForDisplay());
}

TEST_F(TargetDeviceInfoWithImprovedLabelsTest, ActiveTodayWhenNoHighPrecision) {
  base::Time last_updated = base::Time::Now() - base::Minutes(5);
  TargetDeviceInfo device_info(
      "device", "guid", syncer::DeviceInfo::FormFactor::kDesktop, last_updated,
      /*has_high_precision_timestamp=*/false);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_LAST_UPDATE_DAYS, 0),
            device_info.GetLastActiveTimeForDisplay());
}

TEST_F(TargetDeviceInfoWithImprovedLabelsDisabledTest,
       ActiveTodayWhenFlagDisabled) {
  base::Time last_updated = base::Time::Now() - base::Minutes(5);
  TargetDeviceInfo device_info(
      "device", "guid", syncer::DeviceInfo::FormFactor::kDesktop, last_updated,
      /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_LAST_UPDATE_DAYS, 0),
            device_info.GetLastActiveTimeForDisplay());
}

TEST_F(TargetDeviceInfoWithImprovedLabelsDisabledTest, OneDayAgoFallback) {
  base::Time last_updated = base::Time::Now() - base::Days(1) - base::Hours(1);
  TargetDeviceInfo device_info(
      "device", "guid", syncer::DeviceInfo::FormFactor::kDesktop, last_updated,
      /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_LAST_UPDATE_DAYS, 1),
            device_info.GetLastActiveTimeForDisplay());
}

TEST_F(TargetDeviceInfoWithImprovedLabelsDisabledTest,
       MultipleDaysAgoFallback) {
  base::Time last_updated = base::Time::Now() - base::Days(3) - base::Hours(1);
  TargetDeviceInfo device_info(
      "device", "guid", syncer::DeviceInfo::FormFactor::kDesktop, last_updated,
      /*has_high_precision_timestamp=*/false);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_LAST_UPDATE_DAYS, 3),
            device_info.GetLastActiveTimeForDisplay());
}

}  // namespace
}  // namespace send_tab_to_self
