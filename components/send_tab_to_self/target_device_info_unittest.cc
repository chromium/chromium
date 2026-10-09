// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/target_device_info.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_device_info/device_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace send_tab_to_self {
namespace {

using ::testing::Test;
using FormFactor = syncer::DeviceInfo::FormFactor;
using OsType = syncer::DeviceInfo::OsType;

constexpr char kDeviceName[] = "device";
constexpr char kCacheGuid[] = "guid";

class TargetDeviceInfoTest : public Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Verifies that activity within the last minute formats as "Active now".
TEST_F(TargetDeviceInfoTest, ActiveNow) {
  base::Time last_updated = base::Time::Now() - base::Seconds(30);
  TargetDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_NOW),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies that future timestamps (clock skew) clamp to "Active now".
TEST_F(TargetDeviceInfoTest, FutureTimestampClampsToActiveNow) {
  base::Time last_updated = base::Time::Now() + base::Minutes(5);
  TargetDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_NOW),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies plural minute formatting when active several minutes ago.
TEST_F(TargetDeviceInfoTest, ActiveMinutes) {
  base::Time last_updated = base::Time::Now() - base::Minutes(5);
  TargetDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_MINUTES, 5),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies plural hour formatting when active several hours ago.
TEST_F(TargetDeviceInfoTest, ActiveHours) {
  base::Time last_updated = base::Time::Now() - base::Hours(5);
  TargetDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_HOURS, 5),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies singular minute boundary at exactly 1 minute elapsed.
TEST_F(TargetDeviceInfoTest, ActiveOneMinute) {
  base::Time last_updated = base::Time::Now() - base::Minutes(1);
  TargetDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_MINUTES, 1),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies upper minute boundary at 59 minutes elapsed.
TEST_F(TargetDeviceInfoTest, ActiveFiftyNineMinutes) {
  base::Time last_updated = base::Time::Now() - base::Minutes(59);
  TargetDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_MINUTES, 59),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies singular hour boundary at exactly 1 hour elapsed.
TEST_F(TargetDeviceInfoTest, ActiveOneHour) {
  base::Time last_updated = base::Time::Now() - base::Hours(1);
  TargetDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_HOURS, 1),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies upper hour boundary at 23 hours elapsed.
TEST_F(TargetDeviceInfoTest, ActiveTwentyThreeHours) {
  base::Time last_updated = base::Time::Now() - base::Hours(23);
  TargetDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_HOURS, 23),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies day-granularity fallback when `has_high_precision_timestamp` is
// false.
TEST_F(TargetDeviceInfoTest, ActiveTodayWhenNoHighPrecision) {
  base::Time last_updated = base::Time::Now() - base::Minutes(5);
  TargetDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/false);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_LAST_UPDATE_DAYS, 0),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies singular day fallback when elapsed time is at least 1 day.
TEST_F(TargetDeviceInfoTest, OneDayAgoFallback) {
  base::Time last_updated = base::Time::Now() - base::Days(1) - base::Hours(1);
  TargetDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_LAST_UPDATE_DAYS, 1),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies plural days fallback when elapsed time is multiple days.
TEST_F(TargetDeviceInfoTest, MultipleDaysAgoFallback) {
  base::Time last_updated = base::Time::Now() - base::Days(3) - base::Hours(1);
  TargetDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/false);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_LAST_UPDATE_DAYS, 3),
            device_info.GetLastActiveTimeForDisplay());
}

// Tests that the default constructor initializes members to default values.
TEST_F(TargetDeviceInfoTest, DefaultConstructor_InitializesDefaultValues) {
  TargetDeviceInfo device_info;
  EXPECT_TRUE(device_info.device_name.empty());
  EXPECT_TRUE(device_info.cache_guid.empty());
  EXPECT_EQ(FormFactor::kUnknown, device_info.form_factor);
  EXPECT_EQ(OsType::kUnknown, device_info.os_type);
  EXPECT_TRUE(device_info.last_updated_timestamp.is_null());
  EXPECT_FALSE(device_info.has_high_precision_timestamp);
}

}  // namespace
}  // namespace send_tab_to_self
