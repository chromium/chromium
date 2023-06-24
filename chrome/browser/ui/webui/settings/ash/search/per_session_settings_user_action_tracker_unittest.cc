// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/search/per_session_settings_user_action_tracker.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::settings::mojom::Setting;

namespace ash::settings {

constexpr char kProfileName[] = "user@gmail.com";

class PerSessionSettingsUserActionTrackerTest : public testing::Test {
 protected:
  PerSessionSettingsUserActionTrackerTest() = default;
  ~PerSessionSettingsUserActionTrackerTest() override = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    testing_profile_ = profile_manager_->CreateTestingProfile(kProfileName);
    test_pref_service_ = testing_profile_->GetPrefs();
    tracker_ = std::make_unique<PerSessionSettingsUserActionTracker>(
        test_pref_service_);
  }

  void TearDown() override {
    if (tracker_) {
      tracker_.reset();
    }
    profile_manager_->DeleteTestingProfile(kProfileName);
    testing_profile_ = nullptr;
    profile_manager_.reset();
  }

  std::string SettingAsIntString(Setting setting) {
    return base::NumberToString(static_cast<int>(setting));
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  TestingProfile* testing_profile_;
  raw_ptr<PrefService> test_pref_service_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<PerSessionSettingsUserActionTracker> tracker_;
};

TEST_F(PerSessionSettingsUserActionTrackerTest, TestRecordMetrics) {
  // Focus the page, perform some tasks, and change a setting.
  tracker_->RecordPageFocus();
  tracker_->RecordClick();
  tracker_->RecordNavigation();
  tracker_->RecordSearch();
  task_environment_.FastForwardBy(base::Seconds(10));
  tracker_->RecordSettingChange();

  // The "first change" metrics should have been logged.
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumClicksUntilChange.FirstChange",
      /*count=*/1);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumNavigationsUntilChange.FirstChange",
      /*count=*/1);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumSearchesUntilChange.FirstChange",
      /*count=*/1);
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.TimeUntilChange.FirstChange",
      /*sample=*/base::Seconds(10),
      /*count=*/1);

  // Without leaving the page, perform some more tasks, and change another
  // setting.
  tracker_->RecordClick();
  tracker_->RecordNavigation();
  tracker_->RecordSearch();
  task_environment_.FastForwardBy(base::Seconds(10));
  tracker_->RecordSettingChange();

  // The "subsequent change" metrics should have been logged.
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumClicksUntilChange.SubsequentChange",
      /*count=*/1);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumNavigationsUntilChange.SubsequentChange",
      /*count=*/1);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumSearchesUntilChange.SubsequentChange",
      /*count=*/1);
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.TimeUntilChange.SubsequentChange",
      /*sample=*/base::Seconds(10),
      /*count=*/1);

  // Repeat this, but only after 100ms. This is lower than the minimum value
  // required for this metric, so it should be ignored.
  tracker_->RecordClick();
  tracker_->RecordNavigation();
  tracker_->RecordSearch();
  task_environment_.FastForwardBy(base::Milliseconds(100));
  tracker_->RecordSettingChange();

  // No additional logging should have occurred, so make the same verifications
  // as above.
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumClicksUntilChange.SubsequentChange",
      /*count=*/1);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumNavigationsUntilChange.SubsequentChange",
      /*count=*/1);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumSearchesUntilChange.SubsequentChange",
      /*count=*/1);
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.TimeUntilChange.SubsequentChange",
      /*sample=*/base::Seconds(10),
      /*count=*/1);

  // Repeat this once more, and verify that the counts increased.
  tracker_->RecordClick();
  tracker_->RecordNavigation();
  tracker_->RecordSearch();
  task_environment_.FastForwardBy(base::Seconds(10));
  tracker_->RecordSettingChange();

  // The "subsequent change" metrics should have been logged.
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumClicksUntilChange.SubsequentChange",
      /*count=*/2);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumNavigationsUntilChange.SubsequentChange",
      /*count=*/2);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumSearchesUntilChange.SubsequentChange",
      /*count=*/2);
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.TimeUntilChange.SubsequentChange",
      /*sample=*/base::Seconds(10),
      /*count=*/2);
}

TEST_F(PerSessionSettingsUserActionTrackerTest, TestBlurAndFocus) {
  // Focus the page, click, and change a setting.
  tracker_->RecordPageFocus();
  tracker_->RecordClick();
  task_environment_.FastForwardBy(base::Seconds(1));
  tracker_->RecordSettingChange();
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumClicksUntilChange.FirstChange",
      /*count=*/1);
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.TimeUntilChange.FirstChange",
      /*sample=*/base::Seconds(1),
      /*count=*/1);

  // Blur for 59 seconds (not quite a minute), click, and change a setting.
  // Since the blur was under a minute, this should count for the "subsequent
  // change" metrics.
  tracker_->RecordPageBlur();
  task_environment_.FastForwardBy(base::Seconds(59));
  tracker_->RecordPageFocus();
  tracker_->RecordClick();
  tracker_->RecordSettingChange();
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.BlurredWindowDuration",
      /*sample=*/base::Seconds(59),
      /*count=*/1);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumClicksUntilChange.SubsequentChange",
      /*count=*/1);
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.TimeUntilChange.SubsequentChange",
      /*sample=*/base::Seconds(59),
      /*count=*/1);

  // Now, blur for a full minute, click, and change a setting. Since the blur
  // was a full minute, this should count for the "first change" metrics.
  tracker_->RecordPageBlur();
  task_environment_.FastForwardBy(base::Minutes(1));
  tracker_->RecordPageFocus();
  task_environment_.FastForwardBy(base::Seconds(5));
  tracker_->RecordClick();
  tracker_->RecordSettingChange();
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.BlurredWindowDuration",
      /*sample=*/base::Minutes(1),
      /*count=*/2);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumClicksUntilChange.FirstChange",
      /*count=*/2);
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.TimeUntilChange.FirstChange",
      /*sample=*/base::Seconds(5),
      /*count=*/1);
}

TEST_F(PerSessionSettingsUserActionTrackerTest, TestEndSessionWithBlur) {
  // fast forward the time by 30 seconds. Total window active time does not get
  // changed as we have not blurred the session.
  tracker_->RecordPageFocus();
  task_environment_.FastForwardBy(base::Seconds(30));
  EXPECT_EQ(base::TimeDelta(), tracker_->GetTotalTimeSessionActiveForTesting());

  // Total window active time changes to 30 seconds as the page is no longer in
  // focus.
  tracker_->RecordPageBlur();
  EXPECT_EQ(base::Seconds(30), tracker_->GetTotalTimeSessionActiveForTesting());
  // the window is no longer active, so the timer resets.
  EXPECT_EQ(base::TimeTicks(),
            tracker_->GetWindowLastActiveTimeStampForTesting());
}

TEST_F(PerSessionSettingsUserActionTrackerTest, TestUniqueChangedSettings) {
  std::set<std::string> expected_set;

  // Flip the WiFi toggle in Settings, this is a unique Setting that is changing
  // so the number of unique settings that have been changed increases by 1 for
  // a total of 1
  tracker_->RecordSettingChange(Setting::kWifiOnOff);
  expected_set = {SettingAsIntString(Setting::kWifiOnOff)};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_ to trigger recording the data to the histogram.
  tracker_.reset();
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.PerSession",
      /*sample=*/1,
      /*count=*/1);

  // Create a new PerSessionSettingsUserActionTracker to imitate a newly opened
  // Settings page.
  tracker_ =
      std::make_unique<PerSessionSettingsUserActionTracker>(test_pref_service_);

  // test that the set has been destructed and cleared appropriately
  expected_set = {};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Flip the Do Not Disturb and WiFi toggles in Settings, this is a unique
  // Setting that is changing so the number of unique settings that have been
  // changed increases by 1 for a total of 2
  tracker_->RecordSettingChange(Setting::kDoNotDisturbOnOff);
  tracker_->RecordSettingChange(Setting::kWifiOnOff);
  expected_set = {SettingAsIntString(Setting::kDoNotDisturbOnOff),
                  SettingAsIntString(Setting::kWifiOnOff)};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_ to trigger recording the data to the histogram.
  tracker_.reset();
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.PerSession",
      /*sample=*/2,
      /*count=*/1);

  // Create a new PerSessionSettingsUserActionTracker to imitate a newly opened
  // Settings page.
  tracker_ =
      std::make_unique<PerSessionSettingsUserActionTracker>(test_pref_service_);

  // Flip the Do Not Disturb and WiFi toggles. Flip Do Not Disturb toggle again
  // in Settings, this is not a unique Setting that is changing so the number of
  // unique settings that have been changed does not increase. The bucket sample
  // 2 should now have 2 counts.
  tracker_->RecordSettingChange(Setting::kDoNotDisturbOnOff);
  tracker_->RecordSettingChange(Setting::kWifiOnOff);
  tracker_->RecordSettingChange(Setting::kDoNotDisturbOnOff);
  // expected_set will not change
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_ to trigger recording the data to the histogram.
  tracker_.reset();

  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.PerSession",
      /*sample=*/2,
      /*count=*/2);

  // bucket 1 will still reflect the correct number of count added to it
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.PerSession",
      /*sample=*/1,
      /*count=*/1);
}

TEST_F(PerSessionSettingsUserActionTrackerTest,
       TestTotalUniqueChangedSettings) {
  // Simulate that the user has taken OOBE.
  test_pref_service_->SetTime(::ash::prefs::kOobeOnboardingTime,
                              base::Time::Now());

  std::set<std::string> expected_set;

  // Flip the WiFi toggle in Settings, this is a unique Setting that is changing
  // so the number of unique settings that have been changed increases by 1 for
  // a total of 1.
  tracker_->RecordSettingChange(Setting::kWifiOnOff);
  expected_set = {SettingAsIntString(Setting::kWifiOnOff)};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_ to trigger recording the data to the histogram.
  tracker_.reset();
  // The time is still in the first week, so the data gets recorded to
  // .FirstWeek histogram.
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime.FirstWeek",
      /*sample=*/1,
      /*count=*/1);
  // There are no data in the .SubsequentWeeks histogram.
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime."
      "SubsequentWeeks",
      /*sample=*/1,
      /*count=*/0);
  // Overall total unique Settings changed in the lifetime of the Device.
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime.Total",
      /*sample=*/1,
      /*count=*/1);

  // Fast forward the time for 7 days and 1 second. We will now record data to
  // .SubsequentWeeks instead of .FirstWeek.
  task_environment_.FastForwardBy(base::Days(7));
  task_environment_.FastForwardBy(base::Seconds(1));

  // Create a new PerSessionSettingsUserActionTracker to imitate a newly opened
  // Settings page.
  tracker_ =
      std::make_unique<PerSessionSettingsUserActionTracker>(test_pref_service_);

  // test that the set has been destructed and cleared appropriately
  expected_set = {};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Flip the Do Not Disturb toggle twice in Settings. Now that more than 7 days
  // has passed since the user has taken OOBE, this change is a unique Setting
  // that is changing so the number of unique settings in .SubsequentWeeks
  // should increases by 1.
  tracker_->RecordSettingChange(Setting::kDoNotDisturbOnOff);
  expected_set = {SettingAsIntString(Setting::kDoNotDisturbOnOff)};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_ to trigger recording the data to the histogram.
  tracker_.reset();
  // .FirstWeek will not change
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime.FirstWeek",
      /*sample=*/1,
      /*count=*/1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime."
      "SubsequentWeeks",
      /*sample=*/1,
      /*count=*/1);
  // Overall total unique Settings changed in the lifetime of the Device.
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime.Total",
      /*sample=*/1,
      /*count=*/2);

  // Create a new PerSessionSettingsUserActionTracker to imitate a newly opened
  // Settings page.
  tracker_ =
      std::make_unique<PerSessionSettingsUserActionTracker>(test_pref_service_);

  // test that the set has been destructed and cleared appropriately
  expected_set = {};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Flip the Do Not Disturb and WiFi toggles in Settings, this is a unique
  // Setting that is changing so the number of unique settings that have been
  // changed increases by 1. Note that we are still past the 1 week point, so we
  // will add the data to .SubsequentWeeks histogram.
  tracker_->RecordSettingChange(Setting::kDoNotDisturbOnOff);
  tracker_->RecordSettingChange(Setting::kWifiOnOff);
  expected_set = {SettingAsIntString(Setting::kDoNotDisturbOnOff),
                  SettingAsIntString(Setting::kWifiOnOff)};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_ to trigger recording the data to the histogram.
  tracker_.reset();
  // .FirstWeek will not change
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime.FirstWeek",
      /*sample=*/1,
      /*count=*/1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime."
      "SubsequentWeeks",
      /*sample=*/1,
      /*count=*/1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime."
      "SubsequentWeeks",
      /*sample=*/2,
      /*count=*/1);
  // Overall total unique Settings changed in the lifetime of the Device.
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime.Total",
      /*sample=*/1,
      /*count=*/2);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime.Total",
      /*sample=*/2,
      /*count=*/1);

  // Create a new PerSessionSettingsUserActionTracker to imitate a newly opened
  // Settings page.
  tracker_ =
      std::make_unique<PerSessionSettingsUserActionTracker>(test_pref_service_);

  // Flip the Do Not Disturb and WiFi toggles. Flip Do Not Disturb toggle again
  // in Settings, this is not a unique Setting that is changing so the number of
  // unique settings that have been changed does not increase. The bucket sample
  // 2 should now have 2 counts.
  tracker_->RecordSettingChange(Setting::kDoNotDisturbOnOff);
  tracker_->RecordSettingChange(Setting::kWifiOnOff);
  tracker_->RecordSettingChange(Setting::kDoNotDisturbOnOff);
  // expected_set will not change
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_ to trigger recording the data to the histogram.
  tracker_.reset();

  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime.FirstWeek",
      /*sample=*/1,
      /*count=*/1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime."
      "SubsequentWeeks",
      /*sample=*/1,
      /*count=*/1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime."
      "SubsequentWeeks",
      /*sample=*/2,
      /*count=*/1);
  // Overall total unique Settings changed in the lifetime of the Device.
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime.Total",
      /*sample=*/1,
      /*count=*/2);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime.Total",
      /*sample=*/2,
      /*count=*/1);
}

TEST_F(PerSessionSettingsUserActionTrackerTest,
       TestTotalUniqueChangedSettingsWithinFirstWeek) {
  // Simulate that the user has taken OOBE.
  test_pref_service_->SetTime(::ash::prefs::kOobeOnboardingTime,
                              base::Time::Now());
  std::set<std::string> expected_set;

  // Flip the Do Not Disturb and WiFi toggles in Settings, these are unique
  // Settings that are changing so the number of unique settings that have been
  // changed is 2.
  tracker_->RecordSettingChange(Setting::kDoNotDisturbOnOff);
  tracker_->RecordSettingChange(Setting::kWifiOnOff);
  expected_set = {SettingAsIntString(Setting::kDoNotDisturbOnOff),
                  SettingAsIntString(Setting::kWifiOnOff)};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_ to trigger recording the data to the histogram.
  tracker_.reset();
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime.FirstWeek",
      /*sample=*/2,
      /*count=*/1);
  // This is within the first week, no data should be recorded in the
  // .SubsequentWeeks histogram
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime."
      "SubsequentWeeks",
      /*sample=*/2,
      /*count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime.Total",
      /*sample=*/2,
      /*count=*/1);
}

TEST_F(PerSessionSettingsUserActionTrackerTest,
       TestTotalUniqueChangedSettingsAfterFirstWeek) {
  // Simulate that the user has taken OOBE.
  test_pref_service_->SetTime(::ash::prefs::kOobeOnboardingTime,
                              base::Time::Now());
  std::set<std::string> expected_set;
  // Fast forward the time for 7 days and 1 second. We will now record data to
  // .SubsequentWeeks instead of .FirstWeek.
  task_environment_.FastForwardBy(base::Days(16));

  // Flip the Do Not Disturb and WiFi toggles in Settings, these are unique
  // Settings that are changing so the number of unique settings that have been
  // changed is 2.
  tracker_->RecordSettingChange(Setting::kDoNotDisturbOnOff);
  tracker_->RecordSettingChange(Setting::kWifiOnOff);
  expected_set = {SettingAsIntString(Setting::kDoNotDisturbOnOff),
                  SettingAsIntString(Setting::kWifiOnOff)};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_ to trigger recording the data to the histogram.
  tracker_.reset();
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime."
      "SubsequentWeeks",
      /*sample=*/2,
      /*count=*/1);
  // This is after the first week, no data should be recorded in the
  // .FirstWeek histogram
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime.FirstWeek",
      /*sample=*/2,
      /*count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime.Total",
      /*sample=*/2,
      /*count=*/1);
}

TEST_F(PerSessionSettingsUserActionTrackerTest,
       TestNoTimeDeltaOpenCloseSettings) {
  // Focus on page, close the page immediately. total_time_session_active_
  // should be 0 seconds.
  tracker_->RecordPageFocus();

  // Destruct tracker_ to trigger recording the data to the histogram.
  tracker_.reset();

  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.WindowTotalActiveDuration",
      /*sample=*/base::Seconds(0),
      /*count=*/1);
}

TEST_F(PerSessionSettingsUserActionTrackerTest,
       TestTotalTimeSessionActiveWithBlurAndFocus) {
  // Focus on page, wait for 16 seconds to pass, and blur the page.
  // total active time should be 16 seconds.
  tracker_->RecordPageFocus();
  task_environment_.FastForwardBy(base::Seconds(16));
  tracker_->RecordPageBlur();
  EXPECT_EQ(base::TimeTicks(),
            tracker_->GetWindowLastActiveTimeStampForTesting());
  EXPECT_EQ(base::Seconds(16), tracker_->GetTotalTimeSessionActiveForTesting());

  // When the page is blurred, fast forwarding the time would not increase the
  // total active time as the session is not active.
  task_environment_.FastForwardBy(base::Seconds(59));
  EXPECT_EQ(base::TimeTicks(),
            tracker_->GetWindowLastActiveTimeStampForTesting());
  EXPECT_EQ(base::Seconds(16), tracker_->GetTotalTimeSessionActiveForTesting());

  // Focus back on the page, the timer should start up again. wait for 1 minute,
  // now total active time should accumulate to 16 + 60 = 76 seconds.
  tracker_->RecordPageFocus();
  task_environment_.FastForwardBy(base::Minutes(1));
  tracker_->RecordPageBlur();
  EXPECT_EQ(base::TimeTicks(),
            tracker_->GetWindowLastActiveTimeStampForTesting());
  EXPECT_EQ(base::Seconds(76), tracker_->GetTotalTimeSessionActiveForTesting());
  tracker_->RecordPageFocus();

  // Destruct tracker_ to trigger recording the data to the histogram.
  tracker_.reset();

  // Histogram should have 1 count in the 76 seconds bucket.
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.WindowTotalActiveDuration",
      /*sample=*/base::Seconds(76),
      /*count=*/1);
}

TEST_F(PerSessionSettingsUserActionTrackerTest,
       TestMultipleTotalTimeSessionActive) {
  // Focus on page, wait for 22 seconds to pass.
  tracker_->RecordPageFocus();
  task_environment_.FastForwardBy(base::Seconds(22));

  // Destruct tracker_ to trigger recording the data to the histogram.
  tracker_.reset();

  // Histogram should have 1 count in the 22 seconds bucket.
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.WindowTotalActiveDuration",
      /*sample=*/base::Seconds(22),
      /*count=*/1);

  // Create a new tracker, focus on page, wait for another 22 seconds to
  // pass.
  tracker_ =
      std::make_unique<PerSessionSettingsUserActionTracker>(test_pref_service_);
  tracker_->RecordPageFocus();
  task_environment_.FastForwardBy(base::Seconds(22));

  // Destruct tracker_ to trigger recording the data to the histogram.
  tracker_.reset();

  // Histogram should have 2 counts in the 22 seconds bucket.
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.WindowTotalActiveDuration",
      /*sample=*/base::Seconds(22),
      /*count=*/2);

  // Create a new tracker, focus on page, this time wait for 3 seconds to
  // pass.
  tracker_ =
      std::make_unique<PerSessionSettingsUserActionTracker>(test_pref_service_);
  tracker_->RecordPageFocus();
  task_environment_.FastForwardBy(base::Seconds(3));

  // Destruct tracker_ to trigger recording the data to the histogram.
  tracker_.reset();

  // Histogram should have 1 count in the 3 seconds bucket, 2 counts in 22
  // seconds bucket.
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.WindowTotalActiveDuration",
      /*sample=*/base::Seconds(3),
      /*count=*/1);

  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.WindowTotalActiveDuration",
      /*sample=*/base::Seconds(22),
      /*count=*/2);
}

}  // namespace ash::settings
