// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/remote_device_info.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_device_info/test_device_info_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif

namespace syncer {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Test;
using FormFactor = DeviceInfo::FormFactor;
using OsType = DeviceInfo::OsType;

constexpr char kDeviceName[] = "device";
constexpr char kCacheGuid[] = "guid";
constexpr char kLocalGuid[] = "local_guid";
constexpr base::TimeDelta kDefaultMaxAge = base::Days(10);

class RemoteDeviceInfoTest : public Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Verifies that activity within the last minute formats as "Active now".
TEST_F(RemoteDeviceInfoTest, ActiveNow) {
  base::Time last_updated = base::Time::Now() - base::Seconds(30);
  RemoteDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_NOW),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies that future timestamps (clock skew) clamp to "Active now".
TEST_F(RemoteDeviceInfoTest, FutureTimestampClampsToActiveNow) {
  base::Time last_updated = base::Time::Now() + base::Minutes(5);
  RemoteDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_NOW),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies plural minute formatting when active several minutes ago.
TEST_F(RemoteDeviceInfoTest, ActiveMinutes) {
  base::Time last_updated = base::Time::Now() - base::Minutes(5);
  RemoteDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_MINUTES, 5),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies plural hour formatting when active several hours ago.
TEST_F(RemoteDeviceInfoTest, ActiveHours) {
  base::Time last_updated = base::Time::Now() - base::Hours(5);
  RemoteDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_HOURS, 5),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies singular minute boundary at exactly 1 minute elapsed.
TEST_F(RemoteDeviceInfoTest, ActiveOneMinute) {
  base::Time last_updated = base::Time::Now() - base::Minutes(1);
  RemoteDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_MINUTES, 1),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies upper minute boundary at 59 minutes elapsed.
TEST_F(RemoteDeviceInfoTest, ActiveFiftyNineMinutes) {
  base::Time last_updated = base::Time::Now() - base::Minutes(59);
  RemoteDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_MINUTES, 59),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies singular hour boundary at exactly 1 hour elapsed.
TEST_F(RemoteDeviceInfoTest, ActiveOneHour) {
  base::Time last_updated = base::Time::Now() - base::Hours(1);
  RemoteDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_HOURS, 1),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies upper hour boundary at 23 hours elapsed.
TEST_F(RemoteDeviceInfoTest, ActiveTwentyThreeHours) {
  base::Time last_updated = base::Time::Now() - base::Hours(23);
  RemoteDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_HOURS, 23),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies day-granularity fallback when `has_high_precision_timestamp` is
// false.
TEST_F(RemoteDeviceInfoTest, ActiveTodayWhenNoHighPrecision) {
  base::Time last_updated = base::Time::Now() - base::Minutes(5);
  RemoteDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/false);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_LAST_UPDATE_DAYS, 0),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies singular day fallback when elapsed time is at least 1 day.
TEST_F(RemoteDeviceInfoTest, OneDayAgoFallback) {
  base::Time last_updated = base::Time::Now() - base::Days(1) - base::Hours(1);
  RemoteDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/true);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_LAST_UPDATE_DAYS, 1),
            device_info.GetLastActiveTimeForDisplay());
}

// Verifies plural days fallback when elapsed time is multiple days.
TEST_F(RemoteDeviceInfoTest, MultipleDaysAgoFallback) {
  base::Time last_updated = base::Time::Now() - base::Days(3) - base::Hours(1);
  RemoteDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kDesktop,
                               OsType::kLinux, last_updated,
                               /*has_high_precision_timestamp=*/false);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_LAST_UPDATE_DAYS, 3),
            device_info.GetLastActiveTimeForDisplay());
}

// Tests that the default constructor initializes members to default values.
TEST_F(RemoteDeviceInfoTest, DefaultConstructor_InitializesDefaultValues) {
  RemoteDeviceInfo device_info;
  EXPECT_TRUE(device_info.device_name.empty());
  EXPECT_TRUE(device_info.cache_guid.empty());
  EXPECT_EQ(FormFactor::kUnknown, device_info.form_factor);
  EXPECT_EQ(OsType::kUnknown, device_info.os_type);
  EXPECT_TRUE(device_info.last_updated_timestamp.is_null());
  EXPECT_FALSE(device_info.has_high_precision_timestamp);
}

// Verifies that the static `GetLastActiveTimeForDisplay` overload formats both
// high-precision and day-granularity timestamps without constructing a
// `RemoteDeviceInfo` instance.
TEST_F(RemoteDeviceInfoTest, StaticGetLastActiveTimeForDisplay) {
  const base::Time last_updated = base::Time::Now() - base::Minutes(5);

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_MINUTES, 5),
            RemoteDeviceInfo::GetLastActiveTimeForDisplay(
                last_updated, /*has_high_precision_timestamp=*/true));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_SEND_TAB_TO_SELF_DEVICE_LAST_UPDATE_DAYS, 0),
            RemoteDeviceInfo::GetLastActiveTimeForDisplay(
                last_updated, /*has_high_precision_timestamp=*/false));
}

// Verifies that `GetRemoteDeviceInfoSortedList()` returns an empty list when
// `device_info_tracker` tracks no devices.
TEST_F(RemoteDeviceInfoTest,
       GetRemoteDeviceInfoSortedList_EmptyTrackerReturnsEmpty) {
  FakeDeviceInfoTracker tracker;
  EXPECT_THAT(GetRemoteDeviceInfoSortedList(tracker,
                                            /*session_last_modified_times=*/{},
                                            base::Time::Now() - kDefaultMaxAge),
              IsEmpty());
}

// Verifies that `GetRemoteDeviceInfoSortedList()` excludes the local device,
// non-Chrome devices, and expired devices, and sorts remaining remote devices
// by most recent activity descending.
TEST_F(RemoteDeviceInfoTest,
       GetRemoteDeviceInfoSortedList_FiltersAndSortsByRecency) {
  constexpr char kOlderGuid[] = "older_guid";
  constexpr char kOlderName[] = "Older Phone";
  constexpr char kNewerGuid[] = "newer_guid";
  constexpr char kNewerName[] = "Newer Mac";
  const base::Time now = base::Time::Now();
  FakeDeviceInfoTracker tracker;

  tracker.Add(TestDeviceInfoBuilder(OsType::kLinux)
                  .WithGuid(kLocalGuid)
                  .WithClientName("Local Device")
                  .WithLastUpdatedTimestamp(now)
                  .Build());
  tracker.SetLocalCacheGuid(kLocalGuid);

  // Non-Chrome devices have an empty `chrome_version` and are excluded by
  // `GetAllChromeDeviceInfo()`.
  tracker.Add(TestDeviceInfoBuilder(OsType::kAndroid)
                  .WithGuid("non_chrome_guid")
                  .WithChromeVersion("")
                  .WithLastUpdatedTimestamp(now)
                  .Build());

  // Devices inactive for more than `kDefaultMaxAge` are excluded.
  tracker.Add(
      TestDeviceInfoBuilder(OsType::kWindows)
          .WithGuid("expired_guid")
          .WithLastUpdatedTimestamp(now - kDefaultMaxAge - base::Seconds(1))
          .Build());

  tracker.Add(TestDeviceInfoBuilder(OsType::kAndroid)
                  .WithGuid(kOlderGuid)
                  .WithClientName(kOlderName)
                  .WithFormFactor(FormFactor::kPhone)
                  .WithLastUpdatedTimestamp(now - base::Days(2))
                  .Build());

  tracker.Add(TestDeviceInfoBuilder(OsType::kMac)
                  .WithGuid(kNewerGuid)
                  .WithClientName(kNewerName)
                  .WithFormFactor(FormFactor::kDesktop)
                  .WithLastUpdatedTimestamp(now - base::Days(1))
                  .Build());

  EXPECT_THAT(
      GetRemoteDeviceInfoSortedList(tracker,
                                    /*session_last_modified_times=*/{},
                                    now - kDefaultMaxAge),
      ElementsAre(RemoteDeviceInfo(kNewerName, kNewerGuid, FormFactor::kDesktop,
                                   OsType::kMac, now - base::Days(1),
                                   /*has_high_precision_timestamp=*/false),
                  RemoteDeviceInfo(kOlderName, kOlderGuid, FormFactor::kPhone,
                                   OsType::kAndroid, now - base::Days(2),
                                   /*has_high_precision_timestamp=*/false)));
}

// Verifies that `GetRemoteDeviceInfoSortedList()` honors the caller-provided
// `min_last_active_time` threshold, including the exact boundary.
TEST_F(RemoteDeviceInfoTest,
       GetRemoteDeviceInfoSortedList_RespectsMinLastActiveTime) {
  constexpr char kTenDayGuid[] = "ten_day_guid";
  constexpr char kTwentyDayGuid[] = "twenty_day_guid";
  const base::Time now = base::Time::Now();
  FakeDeviceInfoTracker tracker;

  tracker.Add(TestDeviceInfoBuilder(OsType::kLinux)
                  .WithGuid(kTenDayGuid)
                  .WithLastUpdatedTimestamp(now - base::Days(10))
                  .Build());
  tracker.Add(TestDeviceInfoBuilder(OsType::kMac)
                  .WithGuid(kTwentyDayGuid)
                  .WithLastUpdatedTimestamp(now - base::Days(20))
                  .Build());
  tracker.Add(TestDeviceInfoBuilder(OsType::kWindows)
                  .WithGuid("twenty_nine_day_guid")
                  .WithLastUpdatedTimestamp(now - base::Days(29))
                  .Build());

  EXPECT_THAT(GetRemoteDeviceInfoSortedList(tracker,
                                            /*session_last_modified_times=*/{},
                                            now - base::Days(10)),
              ElementsAre(Field(&RemoteDeviceInfo::cache_guid, kTenDayGuid)));

  EXPECT_THAT(
      GetRemoteDeviceInfoSortedList(tracker,
                                    /*session_last_modified_times=*/{},
                                    now - base::Days(28)),
      ElementsAre(Field(&RemoteDeviceInfo::cache_guid, kTenDayGuid),
                  Field(&RemoteDeviceInfo::cache_guid, kTwentyDayGuid)));
}

// Verifies that remote devices with future timestamps caused by clock skew are
// retained and ranked ahead of older devices.
TEST_F(RemoteDeviceInfoTest,
       GetRemoteDeviceInfoSortedList_IncludesFutureTimestampDevice) {
  constexpr char kRecentGuid[] = "recent_guid";
  constexpr char kFutureGuid[] = "future_guid";
  const base::Time now = base::Time::Now();
  FakeDeviceInfoTracker tracker;

  tracker.Add(TestDeviceInfoBuilder(OsType::kLinux)
                  .WithGuid(kRecentGuid)
                  .WithLastUpdatedTimestamp(now - base::Minutes(5))
                  .Build());
  tracker.Add(TestDeviceInfoBuilder(OsType::kMac)
                  .WithGuid(kFutureGuid)
                  .WithLastUpdatedTimestamp(now + base::Hours(2))
                  .Build());

  EXPECT_THAT(GetRemoteDeviceInfoSortedList(tracker,
                                            /*session_last_modified_times=*/{},
                                            now - kDefaultMaxAge),
              ElementsAre(Field(&RemoteDeviceInfo::cache_guid, kFutureGuid),
                          Field(&RemoteDeviceInfo::cache_guid, kRecentGuid)));
}

// Verifies that `GetRemoteDeviceInfoSortedList()` uses the maximum of the
// `DeviceInfo` timestamp and the session timestamp, and marks devices with a
// session timestamp as having a high-precision timestamp even when the
// `DeviceInfo` timestamp is newer.
TEST_F(RemoteDeviceInfoTest,
       GetRemoteDeviceInfoSortedList_UsesMaxOfDeviceAndSessionTimestamps) {
  constexpr char kDevice1Guid[] = "guid1";
  constexpr char kDevice2Guid[] = "guid2";
  constexpr char kDevice1Name[] = "Device 1";
  constexpr char kDevice2Name[] = "Device 2";
  const base::Time now = base::Time::Now();
  FakeDeviceInfoTracker tracker;

  // `kDevice1Guid` has an expired `DeviceInfo` timestamp (11 days ago) and a
  // fresh session timestamp (5 minutes ago), preventing expiration and ranking
  // it first.
  tracker.Add(TestDeviceInfoBuilder(OsType::kAndroid)
                  .WithGuid(kDevice1Guid)
                  .WithClientName(kDevice1Name)
                  .WithFormFactor(FormFactor::kPhone)
                  .WithLastUpdatedTimestamp(now - base::Days(11))
                  .Build());

  // `kDevice2Guid` has a newer `DeviceInfo` timestamp (1 hour ago) than its
  // session timestamp (2 days ago).
  tracker.Add(TestDeviceInfoBuilder(OsType::kLinux)
                  .WithGuid(kDevice2Guid)
                  .WithClientName(kDevice2Name)
                  .WithFormFactor(FormFactor::kDesktop)
                  .WithLastUpdatedTimestamp(now - base::Hours(1))
                  .Build());

  const base::flat_map<std::string, base::Time> session_times = {
      {kDevice1Guid, now - base::Minutes(5)},
      {kDevice2Guid, now - base::Days(2)},
  };

  EXPECT_THAT(
      GetRemoteDeviceInfoSortedList(tracker, session_times,
                                    now - kDefaultMaxAge),
      ElementsAre(
          RemoteDeviceInfo(kDevice1Name, kDevice1Guid, FormFactor::kPhone,
                           OsType::kAndroid, now - base::Minutes(5),
                           /*has_high_precision_timestamp=*/true),
          RemoteDeviceInfo(kDevice2Name, kDevice2Guid, FormFactor::kDesktop,
                           OsType::kLinux, now - base::Hours(1),
                           /*has_high_precision_timestamp=*/true)));
}

// Verifies that `GetRemoteDeviceInfoSortedList()` passes the local `DeviceInfo`
// to `GetDeviceDisplayNames()` so a remote device sharing the local device's
// base display name is disambiguated by release channel when
// `kSyncDisambiguateDeviceNamesWithChannel` is enabled, even if the local
// `DeviceInfo` timestamp is older than `min_last_active_time`.
TEST_F(RemoteDeviceInfoTest,
       GetRemoteDeviceInfoSortedList_DisambiguatesAgainstLocalDeviceChannel) {
  constexpr char kGoogleManufacturer[] = "Google";
  constexpr char kPixel6Model[] = "Pixel 6";
  base::test::ScopedFeatureList scoped_feature_list(
      kSyncDisambiguateDeviceNamesWithChannel);
  const base::Time now = base::Time::Now();
  FakeDeviceInfoTracker tracker;

  tracker.Add(TestDeviceInfoBuilder(OsType::kAndroid)
                  .WithGuid(kLocalGuid)
                  .WithClientName("")
                  .WithManufacturerName(kGoogleManufacturer)
                  .WithModelName(kPixel6Model)
                  .WithSyncUserAgent("Mozilla/5.0 channel(stable)")
                  .WithLastUpdatedTimestamp(now - base::Days(15))
                  .Build());
  tracker.SetLocalCacheGuid(kLocalGuid);

  tracker.Add(TestDeviceInfoBuilder(OsType::kAndroid)
                  .WithGuid("remote_guid")
                  .WithClientName("")
                  .WithManufacturerName(kGoogleManufacturer)
                  .WithModelName(kPixel6Model)
                  .WithSyncUserAgent("Mozilla/5.0 channel(canary)")
                  .WithLastUpdatedTimestamp(now - base::Minutes(1))
                  .Build());

  EXPECT_THAT(GetRemoteDeviceInfoSortedList(tracker,
                                            /*session_last_modified_times=*/{},
                                            now - kDefaultMaxAge),
              ElementsAre(Field(&RemoteDeviceInfo::device_name,
                                "Google Phone (Canary)")));
}

#if BUILDFLAG(IS_ANDROID)
// Verifies that `jni_zero::ToJniType` constructs a non-null Java
// `RemoteDeviceInfo` object.
TEST_F(RemoteDeviceInfoTest, ToJniType) {
  JNIEnv* env = base::android::AttachCurrentThread();
  RemoteDeviceInfo device_info(kDeviceName, kCacheGuid, FormFactor::kPhone,
                               OsType::kAndroid,
                               base::Time::Now() - base::Minutes(5),
                               /*has_high_precision_timestamp=*/true);

  base::android::ScopedJavaLocalRef<jobject> java_device_info =
      jni_zero::ToJniType(env, device_info);
  EXPECT_FALSE(java_device_info.is_null());
}
#endif

}  // namespace
}  // namespace syncer
