// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/search/per_session_settings_user_action_tracker.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

class PerSessionSettingsUserActionTrackerTest : public testing::Test {
 protected:
  PerSessionSettingsUserActionTrackerTest() = default;
  ~PerSessionSettingsUserActionTrackerTest() override = default;

  void SetUp() override {
    tracker_ = std::make_unique<PerSessionSettingsUserActionTracker>();
  }

  void TearDown() override {
    if (tracker_) {
      tracker_.reset();
    }
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
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

TEST_F(PerSessionSettingsUserActionTrackerTest, TestUniqueChangedSettings) {
  std::set<chromeos::settings::mojom::Setting> expected_set;

  // Flip the WiFi toggle in Settings, this is a unique Setting that is changing
  // so the number of unique settings that have been changed increases by 1 for
  // a total of 1
  tracker_->RecordSettingChange(chromeos::settings::mojom::Setting::kWifiOnOff);
  expected_set = {chromeos::settings::mojom::Setting::kWifiOnOff};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_ to trigger recording the data to the histogram.
  tracker_.reset();
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.PerSession",
      /*sample=*/1,
      /*count=*/1);

  // Create a new PerSessionSettingsUserActionTracker to imitate a newly opened
  // Settings page.
  tracker_ = std::make_unique<PerSessionSettingsUserActionTracker>();

  // test that the set has been destructed and cleared appropriately
  expected_set = {};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Flip the Do Not Disturb and WiFi toggles in Settings, this is a unique
  // Setting that is changing so the number of unique settings that have been
  // changed increases by 1 for a total of 2
  tracker_->RecordSettingChange(
      chromeos::settings::mojom::Setting::kDoNotDisturbOnOff);
  tracker_->RecordSettingChange(chromeos::settings::mojom::Setting::kWifiOnOff);
  expected_set = {chromeos::settings::mojom::Setting::kDoNotDisturbOnOff,
                  chromeos::settings::mojom::Setting::kWifiOnOff};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_ to trigger recording the data to the histogram.
  tracker_.reset();
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.PerSession",
      /*sample=*/2,
      /*count=*/1);

  // Create a new PerSessionSettingsUserActionTracker to imitate a newly opened
  // Settings page.
  tracker_ = std::make_unique<PerSessionSettingsUserActionTracker>();

  // Flip the Do Not Disturb and WiFi toggles. Flip Do Not Disturb toggle again
  // in Settings, this is not a unique Setting that is changing so the number of
  // unique settings that have been changed does not increase. The bucket sample
  // 2 should now have 2 counts.
  tracker_->RecordSettingChange(
      chromeos::settings::mojom::Setting::kDoNotDisturbOnOff);
  tracker_->RecordSettingChange(chromeos::settings::mojom::Setting::kWifiOnOff);
  tracker_->RecordSettingChange(
      chromeos::settings::mojom::Setting::kDoNotDisturbOnOff);
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

}  // namespace ash::settings
