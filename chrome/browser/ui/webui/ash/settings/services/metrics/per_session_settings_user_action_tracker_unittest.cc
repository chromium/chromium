// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/services/metrics/per_session_settings_user_action_tracker.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/ui/webui/ash/settings/services/metrics/os_settings_metrics_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/account_id/account_id.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::settings::mojom::Setting;

namespace {

class TestUserMetricsServiceClient
    : public ::metrics::TestMetricsServiceClient {
 public:
  std::optional<bool> GetCurrentUserMetricsConsent() const override {
    return current_user_metrics_consent_;
  }

  void UpdateCurrentUserMetricsConsent(bool metrics_consent) override {
    current_user_metrics_consent_ = metrics_consent;
  }

 private:
  bool current_user_metrics_consent_ = true;
};
}  // namespace

namespace ash::settings {

constexpr char kProfileName[] = "user@gmail.com";

class PerSessionSettingsUserActionTrackerTest : public testing::Test {
 protected:
  PerSessionSettingsUserActionTrackerTest() = default;
  ~PerSessionSettingsUserActionTrackerTest() override = default;

  void SetUp() override {
    std::unique_ptr<FakeChromeUserManager> user_manager =
        std::make_unique<FakeChromeUserManager>();
    FakeChromeUserManager* fake_user_manager = user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    LoginState::Initialize();

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    LoginTestUser(fake_user_manager);

    test_pref_service_ = testing_profile_->GetPrefs();

    // MetricsService.
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    metrics::MetricsService::RegisterPrefs(local_state_->registry());
    test_enabled_state_provider_ =
        std::make_unique<metrics::TestEnabledStateProvider>(true, true);
    test_metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state_.get(), test_enabled_state_provider_.get(), std::wstring(),
        base::FilePath());
    test_metrics_service_client_ =
        std::make_unique<TestUserMetricsServiceClient>();
    test_metrics_service_ = std::make_unique<metrics::MetricsService>(
        test_metrics_state_manager_.get(), test_metrics_service_client_.get(),
        local_state_.get());
    TestingBrowserProcess::GetGlobal()->SetMetricsService(
        test_metrics_service_.get());

    // Needs to be set for metrics service.
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());

    tracker_ = std::make_unique<PerSessionSettingsUserActionTracker>(
        test_pref_service_);

    tracker_metrics_provider_ = std::make_unique<OsSettingsMetricsProvider>();
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetMetricsService(nullptr);

    LoginState::Shutdown();
  }

  // Creates regular_user profile and user then sets that account to active.
  void LoginTestUser(FakeChromeUserManager* fake_user_manager) {
    const AccountId id = AccountId::FromUserEmail(kProfileName);
    auto* user = fake_user_manager->AddUser(id);
    testing_profile_ = profile_manager_->CreateTestingProfile(kProfileName);
    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                            testing_profile_);
    LoginAndSetActiveUserInUserManager(id, fake_user_manager);
  }

  // Helper to set active user in the UserManager.
  void LoginAndSetActiveUserInUserManager(
      const AccountId& id,
      FakeChromeUserManager* fake_user_manager) {
    fake_user_manager->LoginUser(id);
    fake_user_manager->SwitchActiveUser(id);
  }

  std::string SettingAsIntString(Setting setting) {
    return base::NumberToString(static_cast<int>(setting));
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> testing_profile_;
  raw_ptr<PrefService> test_pref_service_;
  std::unique_ptr<PerSessionSettingsUserActionTracker> tracker_;
  std::unique_ptr<OsSettingsMetricsProvider> tracker_metrics_provider_;
  metrics::ChromeUserMetricsExtension uma_proto_;

  // MetricsService.
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<metrics::TestEnabledStateProvider>
      test_enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> test_metrics_state_manager_;
  std::unique_ptr<TestUserMetricsServiceClient> test_metrics_service_client_;
  std::unique_ptr<metrics::MetricsService> test_metrics_service_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
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
  // Simulate that the user has granted UMA consent by default.
  test_pref_service_->SetBoolean(::prefs::kHasEverRevokedMetricsConsent, false);
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
  // Simulate that the user has granted UMA consent by default.
  test_pref_service_->SetBoolean(::prefs::kHasEverRevokedMetricsConsent, false);

  std::set<std::string> expected_set;

  // We will record the total number of unique Settings changed to the histogram
  // ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.{Time}
  // every time a user changes a Setting, no matter if the number of unique
  // Settings used has increased or not.

  // Flip the WiFi toggle in Settings, this is a unique Setting that is changing
  // so the number of unique settings that have been changed increases by 1 for
  // a total of 1.
  tracker_->RecordSettingChange(Setting::kWifiOnOff);
  expected_set = {SettingAsIntString(Setting::kWifiOnOff)};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_.
  tracker_.reset();
  // Trigger recording the data to the histogram. We will now record the unique
  // changes made to .FirstWeek and .Total histogram.
  //
  // .FirstWeek: 1 total unique settings changed with 1 count.
  //
  // .Total: 1 total unique settings changed with 1 count.
  tracker_metrics_provider_->ProvideCurrentSessionData(&uma_proto_);

  // The time is still in the first week, so the data gets recorded to
  // .FirstWeek histogram.
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.FirstWeek",
      /*sample=*/1,
      /*count=*/1);
  // There are no data in the .SubsequentWeeks histogram.
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "SubsequentWeeks",
      /*sample=*/1,
      /*count=*/0);
  // Overall total unique Settings changed in the lifetime of the Device.
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.Total",
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

  // Destruct tracker_ to simulate Settings app closing.
  tracker_.reset();

  // Trigger recording the data to the histogram. We will now record the unique
  // changes made to .SubsequentWeeks and .Total histogram.
  //
  // .SubsequentWeeks: 1 total unique settings changed with 1 count.
  //
  // .Total: We are recording that 1 unique settings has changed, so the count
  // for sample 1 is now accumulated to 2.
  tracker_metrics_provider_->ProvideCurrentSessionData(&uma_proto_);

  // .FirstWeek will not change
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.FirstWeek",
      /*sample=*/1,
      /*count=*/1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "SubsequentWeeks",
      /*sample=*/1,
      /*count=*/1);
  // Overall total unique Settings changed in the lifetime of the Device.
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.Total",
      /*sample=*/1,
      /*count=*/2);

  // Create a new PerSessionSettingsUserActionTracker to imitate a newly opened
  // Settings page.
  tracker_ =
      std::make_unique<PerSessionSettingsUserActionTracker>(test_pref_service_);

  // test that the set has been destructed and cleared appropriately
  expected_set = {};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Simulate that metrics provider uploads the data.
  // Trigger recording the data to the histogram. We will now record the unique
  // changes made to .SubsequentWeeks and .Total histogram.
  //
  // .SubsequentWeeks: 1 total unique settings changed with accumulated 2
  // counts.
  //
  // .Total: We are recording that 1 unique settings has changed, so the count
  // for sample 1 is now accumulated to 3.
  tracker_metrics_provider_->ProvideCurrentSessionData(&uma_proto_);

  // Flip the Do Not Disturb and WiFi toggles in Settings, this is a unique
  // Setting that is changing so the number of unique settings that have been
  // changed increases by 1. Note that we are still past the 1 week point, so we
  // will add the data to .SubsequentWeeks histogram.
  tracker_->RecordSettingChange(Setting::kDoNotDisturbOnOff);
  tracker_->RecordSettingChange(Setting::kWifiOnOff);
  expected_set = {SettingAsIntString(Setting::kDoNotDisturbOnOff),
                  SettingAsIntString(Setting::kWifiOnOff)};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_.
  tracker_.reset();
  // Trigger recording the data to the histogram. We will now record the unique
  // changes made to .SubsequentWeeks and .Total histogram.
  //
  // .SubsequentWeeks: We will now have a new sample since we have 2 different
  // unique settings, so sample 1 remains at 2, and sample 2 now has a new count
  // of 1.
  //
  // .Total: We are recording that 1 unique settings has changed, so the count
  // for sample 1 remains at 3, and sample 2 is now 1.
  tracker_metrics_provider_->ProvideCurrentSessionData(&uma_proto_);

  // .FirstWeek will not change
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.FirstWeek",
      /*sample=*/1,
      /*count=*/1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "SubsequentWeeks",
      /*sample=*/1,
      /*count=*/2);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "SubsequentWeeks",
      /*sample=*/2,
      /*count=*/1);
  // Overall total unique Settings changed in the lifetime of the Device.
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.Total",
      /*sample=*/1,
      /*count=*/3);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.Total",
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

  // Simulate that metrics provider uploads the data.
  //
  // Trigger recording the data to the histogram. We will now record the unique
  // changes made to .SubsequentWeeks and .Total histogram.
  //
  // .SubsequentWeeks: No new unique change has been made, so sample 1 remains
  // at 2, and sample 2 now has a new count of 2.
  //
  // .Total: No new unique change has been made, so the count for sample 1
  // remains at 3, and sample 2 has a new count of 2.
  tracker_metrics_provider_->ProvideCurrentSessionData(&uma_proto_);

  // User is still in the current Settings  session.
  tracker_->RecordSettingChange(Setting::kWifiOnOff);
  tracker_->RecordSettingChange(Setting::kDoNotDisturbOnOff);

  // expected_set will not change
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_ to simulate Settings app closing.
  tracker_.reset();

  // Simulate that metrics provider uploads the data.
  //
  // Trigger recording the data to the histogram. We will now record the unique
  // changes made to .SubsequentWeeks and .Total histogram.
  //
  // .SubsequentWeeks: No new unique change has been made, so sample 1 remains
  // at 2, and sample 2 now has a new count of 3.
  //
  // .Total: No new unique change has been made, so the count for sample 1
  // remains at 3, and sample 2 has a new count of 3.
  tracker_metrics_provider_->ProvideCurrentSessionData(&uma_proto_);

  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.FirstWeek",
      /*sample=*/1,
      /*count=*/1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "SubsequentWeeks",
      /*sample=*/1,
      /*count=*/2);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "SubsequentWeeks",
      /*sample=*/2,
      /*count=*/3);
  // Overall total unique Settings changed in the lifetime of the Device.
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.Total",
      /*sample=*/1,
      /*count=*/3);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.Total",
      /*sample=*/2,
      /*count=*/3);
}

TEST_F(PerSessionSettingsUserActionTrackerTest,
       TestTotalUniqueChangedSettingsWithinFirstWeek) {
  // Simulate that the user has taken OOBE.
  test_pref_service_->SetTime(::ash::prefs::kOobeOnboardingTime,
                              base::Time::Now());
  // Simulate that the user has granted UMA consent by default.
  test_pref_service_->SetBoolean(::prefs::kHasEverRevokedMetricsConsent, false);
  std::set<std::string> expected_set;

  // Flip the Do Not Disturb and WiFi toggles in Settings, these are unique
  // Settings that are changing so the number of unique settings that have been
  // changed is 2.
  tracker_->RecordSettingChange(Setting::kDoNotDisturbOnOff);
  tracker_->RecordSettingChange(Setting::kWifiOnOff);
  expected_set = {SettingAsIntString(Setting::kDoNotDisturbOnOff),
                  SettingAsIntString(Setting::kWifiOnOff)};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_.
  tracker_.reset();
  // Trigger recording the data to the histogram. We will now record the unique
  // changes made to .FirstWeek and .Total histogram.
  //
  // .FirstWeek: 2 total unique settings changed with 1 count.
  //
  // .Total: 2 total unique settings changed with 1 count.
  tracker_metrics_provider_->ProvideCurrentSessionData(&uma_proto_);

  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.FirstWeek",
      /*sample=*/2,
      /*count=*/1);
  // This is within the first week, no data should be recorded in the
  // .SubsequentWeeks histogram
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "SubsequentWeeks",
      /*sample=*/2,
      /*count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.Total",
      /*sample=*/2,
      /*count=*/1);
}

TEST_F(PerSessionSettingsUserActionTrackerTest,
       TestTotalUniqueChangedSettingsAfterFirstWeek) {
  // Simulate that the user has taken OOBE.
  test_pref_service_->SetTime(::ash::prefs::kOobeOnboardingTime,
                              base::Time::Now());
  // Simulate that the user has granted UMA consent by default.
  test_pref_service_->SetBoolean(::prefs::kHasEverRevokedMetricsConsent, false);
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

  // Destruct tracker_.
  tracker_.reset();

  // Trigger recording the data to the histogram. We will now record the unique
  // changes made to .SubsequentWeeks and .Total histogram.
  //
  // .SubsequentWeeks: 2 total unique settings changed with 2 count.
  //
  // .Total: 2 total unique settings changed with 2 count.
  tracker_metrics_provider_->ProvideCurrentSessionData(&uma_proto_);

  // This is after the first week, no data should be recorded in the
  // .FirstWeek histogram
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.FirstWeek",
      /*sample=*/2,
      /*count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "SubsequentWeeks",
      /*sample=*/2,
      /*count=*/1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.Total",
      /*sample=*/2,
      /*count=*/1);
}

TEST_F(PerSessionSettingsUserActionTrackerTest,
       TestNoTimeDeltaOpenCloseSettings) {
  // Simulate that the user has granted UMA consent by default.
  test_pref_service_->SetBoolean(::prefs::kHasEverRevokedMetricsConsent, false);
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
  // Simulate that the user has granted UMA consent by default.
  test_pref_service_->SetBoolean(::prefs::kHasEverRevokedMetricsConsent, false);
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
  // Simulate that the user has granted UMA consent by default.
  test_pref_service_->SetBoolean(::prefs::kHasEverRevokedMetricsConsent, false);
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

TEST_F(PerSessionSettingsUserActionTrackerTest,
       TestRecordedDataWhenUMAConsentRevokedAndRegrantedWithSettingsChange) {
  // Simulate that the user has taken OOBE.
  test_pref_service_->SetTime(::ash::prefs::kOobeOnboardingTime,
                              base::Time::Now());
  // Simulate the initial state, ie the user has not yet set their consent pref.
  test_pref_service_->SetBoolean(::prefs::kHasEverRevokedMetricsConsent, false);

  // Simulate that the user has granted UMA consent initially.
  // NOTE: revoking UMA consent will clear the pref that stores the current
  // unique settings that the user has used. Now that the consent has been
  // granted, we will record that Settings changes to .DeviceLifetime histogram.
  test_metrics_service_client_->UpdateCurrentUserMetricsConsent(true);
  std::set<std::string> expected_set;

  tracker_->RecordPageFocus();

  // Flip the WiFi toggle in Settings, the is a unique Setting that is changing
  // so the number of unique settings that have been changed is 1.
  tracker_->RecordSettingChange(Setting::kWifiOnOff);
  expected_set = {SettingAsIntString(Setting::kWifiOnOff)};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_ to simulate Settings app closing.
  tracker_.reset();

  // Simulate that metrics provider uploads the data.
  //
  // Trigger recording the data to the histogram. We will now record the unique
  // changes made to .FirstWeek and .Total histogram.
  //
  // .FirstWeek: 1 unique change has been made, so sample 1 will have a count
  // of 1.
  // .Total: 1 unique change has been made, so sample 1 will have a count of 1.
  //
  // Other samples in the three histograms .FirstWeek, .SubsequentWeeks, and
  // .Total should have a count of 0 since no data has been recorded to them.
  tracker_metrics_provider_->ProvideCurrentSessionData(&uma_proto_);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.FirstWeek",
      /*sample=*/1,
      /*count=*/1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.FirstWeek",
      /*sample=*/2,
      /*count=*/0);
  // This is within the first week, no data should be recorded in the
  // .SubsequentWeeks histogram
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "SubsequentWeeks",
      /*sample=*/1,
      /*count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "SubsequentWeeks",
      /*sample=*/2,
      /*count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "Total",
      /*sample=*/1,
      /*count=*/1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "Total",
      /*sample=*/2,
      /*count=*/0);

  // Create a new PerSessionSettingsUserActionTracker to imitate a newly opened
  // Settings page.
  tracker_ =
      std::make_unique<PerSessionSettingsUserActionTracker>(test_pref_service_);

  // Simulate that the user has revoked UMA consent, no data will be recorded
  // to UMA.
  test_metrics_service_client_->UpdateCurrentUserMetricsConsent(false);

  // Flip the WiFi toggle in Settings, the is a unique Setting that is changing
  // so the number of unique settings that have been changed is 1.
  tracker_->RecordSettingChange(Setting::kWifiOnOff);
  expected_set = {SettingAsIntString(Setting::kWifiOnOff)};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_.
  tracker_.reset();
  // The metrics provider only gets called to upload the data when the user has
  // granted UMA consent.

  // UMA consent has been revoked so no recording should take place and the
  // histogram data should not change. the data in the histogram does not change
  // since the user has not consented to sharing their info to UMA.
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.FirstWeek",
      /*sample=*/1,
      /*count=*/1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.FirstWeek",
      /*sample=*/2,
      /*count=*/0);
  // This is within the first week, no data should be recorded in the
  // .SubsequentWeeks histogram
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "SubsequentWeeks",
      /*sample=*/1,
      /*count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "SubsequentWeeks",
      /*sample=*/2,
      /*count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "Total",
      /*sample=*/1,
      /*count=*/1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "Total",
      /*sample=*/2,
      /*count=*/0);

  // Create a new PerSessionSettingsUserActionTracker to imitate a newly opened
  // Settings page.
  tracker_ =
      std::make_unique<PerSessionSettingsUserActionTracker>(test_pref_service_);

  // Simulate that the user has re-granted UMA consent, no data will be recorded
  // to UMA because they have previously revoked their consent and have made a
  // change in Settings. The data we are recording is no longer accurate so we
  // will no longer record the data to UMA. See
  // ::prefs::kHasEverRevokedMetricsConsent for more information.
  test_metrics_service_client_->UpdateCurrentUserMetricsConsent(true);

  // Flip the WiFi toggle in Settings, the is a unique Setting that is changing
  // so the number of unique settings that have been changed is 1.
  tracker_->RecordSettingChange(Setting::kWifiOnOff);
  expected_set = {SettingAsIntString(Setting::kWifiOnOff)};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_.
  tracker_.reset();
  // Simulate that metrics provider uploads the data.
  //
  // Trigger recording the data to the histogram. UMA consent has been revoked
  // before, so no recording should take place and the histogram data should not
  // change.
  //
  // .FirstWeek: Sample 1 will remain at count of 1.
  // .Total: Sample 1 will remain at count of 1.
  //
  // Other samples in the three histograms .FirstWeek, .SubsequentWeeks, and
  // .Total should have a count of 0 since no data has been recorded to them.
  tracker_metrics_provider_->ProvideCurrentSessionData(&uma_proto_);

  // the data in the histogram does not change since the user has revoked their
  // consent for UMA at least once in the lifetime of their device.
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.FirstWeek",
      /*sample=*/0,
      /*count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.FirstWeek",
      /*sample=*/1,
      /*count=*/1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.FirstWeek",
      /*sample=*/2,
      /*count=*/0);
  // This is within the first week, no data should be recorded in the
  // .SubsequentWeeks histogram
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "SubsequentWeeks",
      /*sample=*/0,
      /*count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "SubsequentWeeks",
      /*sample=*/1,
      /*count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "SubsequentWeeks",
      /*sample=*/2,
      /*count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "Total",
      /*sample=*/0,
      /*count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "Total",
      /*sample=*/1,
      /*count=*/1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "Total",
      /*sample=*/2,
      /*count=*/0);
}

TEST_F(PerSessionSettingsUserActionTrackerTest,
       TestRecordedDataWhenUMAConsentRevokedAndRegrantedWithoutSettingsChange) {
  // Simulate that the user has taken OOBE.
  test_pref_service_->SetTime(::ash::prefs::kOobeOnboardingTime,
                              base::Time::Now());
  // Simulate the initial state, ie the user has not yet set their consent pref.
  test_pref_service_->SetBoolean(::prefs::kHasEverRevokedMetricsConsent, false);

  // Simulate that the user has granted UMA consent initially.
  // NOTE: revoking UMA consent will clear the pref that stores the current
  // unique settings that the user has used. Now that the consent has been
  // granted, we will record that Settings changes to .DeviceLifetime histogram.
  test_metrics_service_client_->UpdateCurrentUserMetricsConsent(true);
  std::set<std::string> expected_set;

  // Simulate that the user has revoked UMA consent, the function
  // ProvideCurrentSessionData will not get called and no data will be recorded
  // to UMA.
  test_metrics_service_client_->UpdateCurrentUserMetricsConsent(false);

  // Simulate that the user has re-granted UMA consent, no data will be recorded
  // to UMA because they have previously revoked their consent. Once a user
  // revokes their consent, re-granting it without making a Setting change will
  // result in recording of their data, since we will not have any discrepancy
  // in the data we are recording.
  test_metrics_service_client_->UpdateCurrentUserMetricsConsent(true);

  // Flip the WiFi toggle in Settings, the is a unique Setting that is changing
  // so the number of unique settings that have been changed is 1.
  tracker_->RecordSettingChange(Setting::kWifiOnOff);
  expected_set = {SettingAsIntString(Setting::kWifiOnOff)};
  EXPECT_EQ(expected_set, tracker_->GetChangedSettingsForTesting());

  // Destruct tracker_.
  tracker_.reset();

  // Simulate that metrics provider uploads the data.
  //
  // .FirstWeek: Sample 1 will have a count of 1
  // .Total: Sample 1 will have a count of 1.
  //
  // All other samples in the three histograms .FirstWeek, .SubsequentWeeks, and
  // .Total should have a count of 0 since no data has been recorded to them.
  tracker_metrics_provider_->ProvideCurrentSessionData(&uma_proto_);

  // the data in the histogram does not change since the user has revoked their
  // consent for UMA at least once in the lifetime of their device.
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.FirstWeek",
      /*sample=*/0,
      /*count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.FirstWeek",
      /*sample=*/1,
      /*count=*/1);
  // This is within the first week, no data should be recorded in the
  // .SubsequentWeeks histogram
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "SubsequentWeeks",
      /*sample=*/0,
      /*count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "SubsequentWeeks",
      /*sample=*/1,
      /*count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "Total",
      /*sample=*/0,
      /*count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
      "Total",
      /*sample=*/1,
      /*count=*/1);
}

}  // namespace ash::settings
