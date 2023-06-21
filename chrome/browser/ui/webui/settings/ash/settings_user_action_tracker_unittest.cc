// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/settings_user_action_tracker.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/settings/ash/fake_hierarchy.h"
#include "chrome/browser/ui/webui/settings/ash/fake_os_settings_section.h"
#include "chrome/browser/ui/webui/settings/ash/fake_os_settings_sections.h"
#include "chrome/browser/ui/webui/settings/ash/search/per_session_settings_user_action_tracker.h"
#include "chrome/browser/ui/webui/settings/ash/search/user_action_recorder.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/setting.mojom.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

constexpr char kProfileName[] = "user@gmail.com";

namespace mojom {
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
}  // namespace mojom

class SettingsUserActionTrackerTest : public testing::Test {
 protected:
  SettingsUserActionTrackerTest()
      : fake_hierarchy_(&fake_sections_),
        tracker_(&fake_hierarchy_, &fake_sections_, GetTestProfilePref()) {
    // Initialize per_session_tracker_ manually since BindInterface is never
    // called on tracker_.
    tracker_.per_session_tracker_ =
        std::make_unique<PerSessionSettingsUserActionTracker>(
            testing_profile_->GetPrefs());
  }
  ~SettingsUserActionTrackerTest() override = default;

  void SetUpTestingProfile() {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    testing_profile_ = profile_manager_->CreateTestingProfile(kProfileName);
  }

  PrefService* GetTestProfilePref() {
    SetUpTestingProfile();
    test_pref_service_ = testing_profile_->GetPrefs();
    return test_pref_service_;
  }

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
    fake_hierarchy_.AddSettingMetadata(mojom::Section::kNetwork,
                                       mojom::Setting::kWifiAddNetwork);
  }

  void TearDown() override { tracker_.per_session_tracker_.reset(); }

  // TestingProfile is bound to the IO thread:
  // CurrentlyOn(content::BrowserThread::UI).
  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  FakeOsSettingsSections fake_sections_;
  raw_ptr<PrefService> test_pref_service_;
  FakeHierarchy fake_hierarchy_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  SettingsUserActionTracker tracker_;
  TestingProfile* testing_profile_;
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

TEST_F(SettingsUserActionTrackerTest, TestRecordSettingChangedString) {
  // Record that the user tried to add a 3rd account.
  tracker_.RecordSettingChangeWithDetails(
      mojom::Setting::kWifiAddNetwork,
      mojom::SettingChangeValue::NewStringValue("guid"));

  // The umbrella metric for which setting was changed should be updated. Note
  // that kWifiAddNetwork has enum value of 8.
  histogram_tester_.ExpectTotalCount("ChromeOS.Settings.SettingChanged",
                                     /*count=*/1);
  histogram_tester_.ExpectBucketCount("ChromeOS.Settings.SettingChanged",
                                      /*sample=*/8,
                                      /*count=*/1);

  // The LogMetric fn in the People section should have been called.
  const FakeOsSettingsSection* network_section =
      static_cast<const FakeOsSettingsSection*>(
          fake_sections_.GetSection(mojom::Section::kNetwork));
  EXPECT_TRUE(network_section->logged_metrics().back() ==
              mojom::Setting::kWifiAddNetwork);
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

}  // namespace ash::settings
