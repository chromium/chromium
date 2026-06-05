// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/cross_device/cross_device_theme_tracker.h"

#include "base/test/task_environment.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  explicit TestCrossDeviceThemeTracker(
      syncer::DeviceInfoTracker* device_info_tracker)
      : CrossDeviceThemeTracker(device_info_tracker) {}

  using CrossDeviceThemeTracker::RemoveThemeInfo;
  using CrossDeviceThemeTracker::SetStatus;
  using CrossDeviceThemeTracker::UpdateThemeInfo;
};

class CrossDeviceThemeTrackerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  syncer::FakeDeviceInfoTracker fake_device_info_tracker_;
  TestCrossDeviceThemeTracker tracker_{&fake_device_info_tracker_};
};

TEST_F(CrossDeviceThemeTrackerTest, InitialState) {
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kInitializing);
  EXPECT_TRUE(tracker_.GetOtherDevicesThemes().empty());
}

TEST_F(CrossDeviceThemeTrackerTest, UpdateAndRemoveTheme) {
  MockObserver observer;
  tracker_.AddObserver(&observer);

  PlatformThemeInfo theme_info;
  theme_info.device_name = "Phone";
  theme_info.os_type = syncer::DeviceInfo::OsType::kAndroid;
  theme_info.form_factor = syncer::DeviceInfo::FormFactor::kPhone;
  theme_info.color = SK_ColorBLUE;

  // Expect observer notification on update.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  tracker_.UpdateThemeInfo("guid_1", theme_info);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Verify theme is in the list.
  auto themes = tracker_.GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  EXPECT_EQ(themes[0].device_name, "Phone");
  EXPECT_EQ(themes[0].os_type, syncer::DeviceInfo::OsType::kAndroid);
  EXPECT_EQ(themes[0].form_factor, syncer::DeviceInfo::FormFactor::kPhone);
  EXPECT_EQ(themes[0].color, SK_ColorBLUE);

  // Update same guid.
  theme_info.color = SK_ColorRED;
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  tracker_.UpdateThemeInfo("guid_1", theme_info);
  testing::Mock::VerifyAndClearExpectations(&observer);

  themes = tracker_.GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  EXPECT_EQ(themes[0].color, SK_ColorRED);

  // Update with same info, expect NO notification.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(0);
  tracker_.UpdateThemeInfo("guid_1", theme_info);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Add another guid.
  PlatformThemeInfo theme_info2;
  theme_info2.device_name = "Tablet";
  theme_info2.os_type = syncer::DeviceInfo::OsType::kAndroid;
  theme_info2.form_factor = syncer::DeviceInfo::FormFactor::kTablet;
  theme_info2.color = SK_ColorGREEN;

  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  tracker_.UpdateThemeInfo("guid_2", theme_info2);
  testing::Mock::VerifyAndClearExpectations(&observer);

  themes = tracker_.GetOtherDevicesThemes();
  EXPECT_EQ(themes.size(), 2u);

  // Remove one.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  tracker_.RemoveThemeInfo("guid_1");
  testing::Mock::VerifyAndClearExpectations(&observer);

  themes = tracker_.GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  EXPECT_EQ(themes[0].device_name, "Tablet");

  // Remove non-existent.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(0);
  tracker_.RemoveThemeInfo("guid_non_existent");
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

}  // namespace

}  // namespace themes
