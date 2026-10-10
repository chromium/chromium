// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/cross_device/cross_device_theme_tracker.h"

#include "base/test/task_environment.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/themes/cross_device/theme_comparer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace themes {

namespace {

class MockObserver
    : public CrossDeviceThemeTracker<sync_pb::ThemeSpecifics>::Observer {
 public:
  MOCK_METHOD(void, OnCrossDeviceThemeChanged, (), (override));
  MOCK_METHOD(void, OnServiceStatusChanged, (ServiceStatus), (override));
};

// Testable subclass to expose protected methods.
class TestCrossDeviceThemeTracker
    : public CrossDeviceThemeTracker<sync_pb::ThemeSpecifics> {
 public:
  TestCrossDeviceThemeTracker() = default;

  using CrossDeviceThemeTracker::RemoveThemeInfo;
  using CrossDeviceThemeTracker::SetStatus;
  using CrossDeviceThemeTracker::UpdateThemeInfo;

  void SetBridgeStatusForTesting(syncer::DataType type, ServiceStatus status) {
    this->bridge_statuses_[type] = status;
    this->UpdateAggregateStatus();
  }
};

class CrossDeviceThemeTrackerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  TestCrossDeviceThemeTracker tracker_;
};

TEST_F(CrossDeviceThemeTrackerTest, InitialState) {
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kInitializing);
  EXPECT_TRUE(tracker_.GetOtherDevicesThemes().empty());
}

TEST_F(CrossDeviceThemeTrackerTest, UpdateAndRemoveTheme) {
  MockObserver observer;
  tracker_.AddObserver(&observer);

  DeviceThemeInfo<sync_pb::ThemeSpecifics> theme_info;
  theme_info.data_type = syncer::THEMES;
  theme_info.theme.mutable_user_color_theme()->set_color(SK_ColorBLUE);

  // Expect observer notification on update.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  tracker_.UpdateThemeInfo("current_theme", theme_info);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Verify theme is in the list.
  auto themes = tracker_.GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  EXPECT_EQ(themes[0].data_type, syncer::THEMES);
  ASSERT_TRUE(themes[0].theme.has_user_color_theme());
  EXPECT_EQ(themes[0].theme.user_color_theme().color(), SK_ColorBLUE);

  // Update same tag.
  theme_info.theme.mutable_user_color_theme()->set_color(SK_ColorRED);
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  tracker_.UpdateThemeInfo("current_theme", theme_info);
  testing::Mock::VerifyAndClearExpectations(&observer);

  themes = tracker_.GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  ASSERT_TRUE(themes[0].theme.has_user_color_theme());
  EXPECT_EQ(themes[0].theme.user_color_theme().color(), SK_ColorRED);

  // Update with same info, expect NO notification.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(0);
  tracker_.UpdateThemeInfo("current_theme", theme_info);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Add another tag.
  DeviceThemeInfo<sync_pb::ThemeSpecifics> theme_info2;
  theme_info2.data_type = syncer::THEMES_IOS;
  theme_info2.theme.mutable_user_color_theme()->set_color(SK_ColorGREEN);

  // Expect observer notification on update.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  tracker_.UpdateThemeInfo("current_theme_ios", theme_info2);
  testing::Mock::VerifyAndClearExpectations(&observer);

  themes = tracker_.GetOtherDevicesThemes();
  EXPECT_EQ(themes.size(), 2u);

  // Remove one.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  tracker_.RemoveThemeInfo("current_theme");
  testing::Mock::VerifyAndClearExpectations(&observer);

  themes = tracker_.GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  EXPECT_EQ(themes[0].data_type, syncer::THEMES_IOS);

  // Remove non-existent.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(0);
  tracker_.RemoveThemeInfo("tag_non_existent");
  testing::Mock::VerifyAndClearExpectations(&observer);

  tracker_.RemoveObserver(&observer);
}

TEST_F(CrossDeviceThemeTrackerTest, StatusChanges) {
  MockObserver observer;
  tracker_.AddObserver(&observer);

  EXPECT_CALL(observer, OnServiceStatusChanged(ServiceStatus::kActive))
      .Times(1);
  tracker_.SetStatus(ServiceStatus::kActive);
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kActive);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Set same status, no notification.
  EXPECT_CALL(observer, OnServiceStatusChanged(testing::_)).Times(0);
  tracker_.SetStatus(ServiceStatus::kActive);
  testing::Mock::VerifyAndClearExpectations(&observer);

  tracker_.RemoveObserver(&observer);
}

TEST_F(CrossDeviceThemeTrackerTest, BridgeStatusAggregation) {
  MockObserver observer;
  tracker_.AddObserver(&observer);

  // Initial status is kInitializing.
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kInitializing);

  // One bridge becomes active -> kActive.
  EXPECT_CALL(observer, OnServiceStatusChanged(ServiceStatus::kActive))
      .Times(1);
  tracker_.SetBridgeStatusForTesting(syncer::THEMES_ANDROID,
                                     ServiceStatus::kActive);
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kActive);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Other bridge becomes disabled -> still kActive (since Android is active).
  EXPECT_CALL(observer, OnServiceStatusChanged(testing::_)).Times(0);
  tracker_.SetBridgeStatusForTesting(syncer::THEMES_IOS,
                                     ServiceStatus::kSyncDisabled);
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kActive);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Android also becomes disabled -> all disabled -> kSyncDisabled.
  EXPECT_CALL(observer, OnServiceStatusChanged(ServiceStatus::kSyncDisabled))
      .Times(1);
  tracker_.SetBridgeStatusForTesting(syncer::THEMES_ANDROID,
                                     ServiceStatus::kSyncDisabled);
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kSyncDisabled);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Android becomes initializing -> kInitializing.
  EXPECT_CALL(observer, OnServiceStatusChanged(ServiceStatus::kInitializing))
      .Times(1);
  tracker_.SetBridgeStatusForTesting(syncer::THEMES_ANDROID,
                                     ServiceStatus::kInitializing);
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kInitializing);
  testing::Mock::VerifyAndClearExpectations(&observer);

  tracker_.RemoveObserver(&observer);
}

TEST_F(CrossDeviceThemeTrackerTest, OsTypeToDataType) {
  EXPECT_EQ(OsTypeToDataType(syncer::DeviceInfo::OsType::kAndroid,
                             syncer::DeviceInfo::FormFactor::kPhone),
            syncer::THEMES_ANDROID);
  EXPECT_EQ(OsTypeToDataType(syncer::DeviceInfo::OsType::kAndroid,
                             syncer::DeviceInfo::FormFactor::kDesktop),
            syncer::THEMES);
  EXPECT_EQ(OsTypeToDataType(syncer::DeviceInfo::OsType::kIOS,
                             syncer::DeviceInfo::FormFactor::kPhone),
            syncer::THEMES_IOS);
  EXPECT_EQ(OsTypeToDataType(syncer::DeviceInfo::OsType::kWindows,
                             syncer::DeviceInfo::FormFactor::kDesktop),
            syncer::THEMES);
  EXPECT_EQ(OsTypeToDataType(syncer::DeviceInfo::OsType::kMac,
                             syncer::DeviceInfo::FormFactor::kDesktop),
            syncer::THEMES);
  EXPECT_EQ(OsTypeToDataType(syncer::DeviceInfo::OsType::kLinux,
                             syncer::DeviceInfo::FormFactor::kDesktop),
            syncer::THEMES);
  EXPECT_EQ(OsTypeToDataType(syncer::DeviceInfo::OsType::kChromeOsAsh,
                             syncer::DeviceInfo::FormFactor::kDesktop),
            syncer::THEMES);
  EXPECT_EQ(OsTypeToDataType(syncer::DeviceInfo::OsType::kUnknown,
                             syncer::DeviceInfo::FormFactor::kUnknown),
            syncer::UNSPECIFIED);
}

TEST_F(CrossDeviceThemeTrackerTest,
       OnBridgeSyncDisabledClearsThemesForDataType) {
  tracker_.OnBridgeSyncStarted(syncer::THEMES);
  tracker_.OnBridgeSyncStarted(syncer::THEMES_IOS);
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kActive);

  DeviceThemeInfo<sync_pb::ThemeSpecifics> desktop_theme;
  desktop_theme.data_type = syncer::THEMES;
  desktop_theme.theme.mutable_user_color_theme()->set_color(SK_ColorBLUE);
  tracker_.UpdateThemeInfo("current_theme", desktop_theme);

  DeviceThemeInfo<sync_pb::ThemeSpecifics> ios_theme;
  ios_theme.data_type = syncer::THEMES_IOS;
  ios_theme.theme.mutable_user_color_theme()->set_color(SK_ColorGREEN);
  tracker_.UpdateThemeInfo("current_theme_ios", ios_theme);

  ASSERT_EQ(tracker_.GetOtherDevicesThemes().size(), 2u);

  tracker_.OnBridgeSyncDisabled(syncer::THEMES);

  auto themes = tracker_.GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  EXPECT_EQ(themes[0].data_type, syncer::THEMES_IOS);
}

}  // namespace

}  // namespace themes
