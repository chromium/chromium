// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/settings_user_action_tracker.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/setting.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/fake_hierarchy.h"
#include "chrome/browser/ui/webui/settings/chromeos/fake_os_settings_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/fake_os_settings_sections.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/per_session_settings_user_action_tracker.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/user_action_recorder.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

class SettingsUserActionTrackerTest : public testing::Test {
 protected:
  SettingsUserActionTrackerTest()
      : fake_hierarchy_(&fake_sections_),
        tracker_(&fake_hierarchy_, &fake_sections_) {
    // Initialize per_session_tracker_ manually since BindInterface is never
    // called on tracker_.
    tracker_.per_session_tracker_ =
        std::make_unique<PerSessionSettingsUserActionTracker>();
  }
  ~SettingsUserActionTrackerTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_hierarchy_.AddSettingMetadata(mojom::Section::kBluetooth,
                                       mojom::Setting::kBluetoothOnOff);
    fake_hierarchy_.AddSettingMetadata(mojom::Section::kDevice,
                                       mojom::Setting::kKeyboardFunctionKeys);
    fake_hierarchy_.AddSettingMetadata(mojom::Section::kDevice,
                                       mojom::Setting::kTouchpadSpeed);
    fake_hierarchy_.AddSettingMetadata(mojom::Section::kPeople,
                                       mojom::Setting::kAddAccount);
    fake_hierarchy_.AddSettingMetadata(mojom::Section::kPrinting,
                                       mojom::Setting::kScanningApp);
  }

  base::HistogramTester histogram_tester_;
  FakeOsSettingsSections fake_sections_;
  FakeHierarchy fake_hierarchy_;
  SettingsUserActionTracker tracker_;
};

TEST_F(SettingsUserActionTrackerTest, TestRecordSettingChangedBool) {
  // Record that the bluetooth enabled setting was toggled off.
  tracker_.RecordSettingChangeWithDetails(
      mojom::Setting::kBluetoothOnOff,
      mojom::SettingChangeValue::NewBoolValue(false));

  // The umbrella metric for which setting was changed should be updated. Note
  // that kBluetoothOnOff has enum value of 100.
  histogram_tester_.ExpectTotalCount("ChromeOS.Settings.SettingChanged",
                                     /*count=*/1);
  histogram_tester_.ExpectBucketCount("ChromeOS.Settings.SettingChanged",
                                      /*sample=*/100,
                                      /*count=*/1);

  // The LogMetric fn in the Blutooth section should have been called.
  const FakeOsSettingsSection* bluetooth_section =
      static_cast<const FakeOsSettingsSection*>(
          fake_sections_.GetSection(mojom::Section::kBluetooth));
  EXPECT_TRUE(bluetooth_section->logged_metrics().back() ==
              mojom::Setting::kBluetoothOnOff);
}

TEST_F(SettingsUserActionTrackerTest, TestRecordSettingChangedInt) {
  // Record that the user tried to add a 3rd account.
  tracker_.RecordSettingChangeWithDetails(
      mojom::Setting::kAddAccount, mojom::SettingChangeValue::NewIntValue(3));

  // The umbrella metric for which setting was changed should be updated. Note
  // that kAddAccount has enum value of 300.
  histogram_tester_.ExpectTotalCount("ChromeOS.Settings.SettingChanged",
                                     /*count=*/1);
  histogram_tester_.ExpectBucketCount("ChromeOS.Settings.SettingChanged",
                                      /*sample=*/300,
                                      /*count=*/1);

  // The LogMetric fn in the People section should have been called.
  const FakeOsSettingsSection* people_section =
      static_cast<const FakeOsSettingsSection*>(
          fake_sections_.GetSection(mojom::Section::kPeople));
  EXPECT_TRUE(people_section->logged_metrics().back() ==
              mojom::Setting::kAddAccount);
}

TEST_F(SettingsUserActionTrackerTest, TestRecordSettingChangedBoolPref) {
  // Record that the keyboard function keys setting was toggled off. This
  // setting is controlled by a pref and uses the pref-to-setting-metric
  // converter flow.
  tracker_.RecordSettingChangeWithDetails(
      mojom::Setting::kKeyboardFunctionKeys,
      mojom::SettingChangeValue::NewBoolValue(false));

  // The umbrella metric for which setting was changed should be updated. Note
  // that kKeyboardFunctionKeys has enum value of 411.
  histogram_tester_.ExpectTotalCount("ChromeOS.Settings.SettingChanged",
                                     /*count=*/1);
  histogram_tester_.ExpectBucketCount("ChromeOS.Settings.SettingChanged",
                                      /*sample=*/411,
                                      /*count=*/1);

  // The LogMetric fn in the Device section should have been called.
  const FakeOsSettingsSection* device_section =
      static_cast<const FakeOsSettingsSection*>(
          fake_sections_.GetSection(mojom::Section::kDevice));
  EXPECT_TRUE(device_section->logged_metrics().back() ==
              mojom::Setting::kKeyboardFunctionKeys);
}

TEST_F(SettingsUserActionTrackerTest, TestRecordSettingChangedIntPref) {
  // Record that the touchpad speed slider was changed by the user. This
  // setting is controlled by a pref and uses the pref-to-setting-metric
  // converter flow.
  tracker_.RecordSettingChangeWithDetails(
      mojom::Setting::kTouchpadSpeed,
      mojom::SettingChangeValue::NewIntValue(4));

  // The umbrella metric for which setting was changed should be updated. Note
  // that kTouchpadSpeed has enum value of 405.
  histogram_tester_.ExpectTotalCount("ChromeOS.Settings.SettingChanged",
                                     /*count=*/1);
  histogram_tester_.ExpectBucketCount("ChromeOS.Settings.SettingChanged",
                                      /*sample=*/405,
                                      /*count=*/1);

  // The LogMetric fn in the Device section should have been called.
  const FakeOsSettingsSection* device_section =
      static_cast<const FakeOsSettingsSection*>(
          fake_sections_.GetSection(mojom::Section::kDevice));
  EXPECT_TRUE(device_section->logged_metrics().back() ==
              mojom::Setting::kTouchpadSpeed);
}

TEST_F(SettingsUserActionTrackerTest, TestRecordSettingChangedNullValue) {
  // Record that the Scan app is opened.
  tracker_.RecordSettingChangeWithDetails(mojom::Setting::kScanningApp,
                                          nullptr);

  // The umbrella metric for which setting was changed should be updated. Note
  // that kScanningApp has enum value of 1403.
  histogram_tester_.ExpectTotalCount("ChromeOS.Settings.SettingChanged",
                                     /*count=*/1);
  histogram_tester_.ExpectBucketCount("ChromeOS.Settings.SettingChanged",
                                      /*sample=*/1403,
                                      /*count=*/1);

  // The LogMetric fn in the Printing section should have been called.
  const FakeOsSettingsSection* printing_section =
      static_cast<const FakeOsSettingsSection*>(
          fake_sections_.GetSection(mojom::Section::kPrinting));
  EXPECT_TRUE(printing_section->logged_metrics().back() ==
              mojom::Setting::kScanningApp);
}

}  // namespace settings.
}  // namespace chromeos.
