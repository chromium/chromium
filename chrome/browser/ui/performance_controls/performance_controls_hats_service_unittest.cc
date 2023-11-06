// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/performance_controls_hats_service.h"
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/power_monitor_test_utils.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/test_support/test_user_performance_tuning_manager_environment.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

class PerformanceControlsHatsServiceTest : public testing::Test {
 public:
  PerformanceControlsHatsServiceTest() = default;

  void SetUp() override {
    testing::Test::SetUp();

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    TestingProfile* profile =
        profile_manager_->CreateTestingProfile("Test", true);

    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile, base::BindRepeating(&BuildMockHatsService)));
    EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(_))
        .WillRepeatedly(testing::Return(true));

    feature_list_.InitWithFeaturesAndParameters(GetFeatures(), {});
    performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(
        local_state_.registry());
    environment_.SetUp(&local_state_);

    performance_controls_hats_service_ =
        std::make_unique<PerformanceControlsHatsService>(&local_state_,
                                                         profile);
  }

  void TearDown() override {
    testing::Test::TearDown();
    // The service has to be destroyed before the UserPerformanceTuningManager
    // is destroyed by `environment_.TearDown()`, otherwise the service will try
    // to unregister as an observer on a freed UserPerformanceTuningManager.
    performance_controls_hats_service_.reset();
    environment_.TearDown();
  }

  void SetBatterySaverMode(
      const performance_manager::user_tuning::prefs::BatterySaverModeState
          battery_saver_mode) {
    local_state()->SetInteger(
        performance_manager::user_tuning::prefs::kBatterySaverModeState,
        static_cast<int>(battery_saver_mode));
  }

  void SetHighEfficiencyEnabled(const bool high_efficiency_enabled) {
    performance_manager::user_tuning::UserPerformanceTuningManager::
        GetInstance()
            ->SetHighEfficiencyModeEnabled(high_efficiency_enabled);
  }

  PerformanceControlsHatsService* performance_controls_hats_service() {
    return performance_controls_hats_service_.get();
  }
  MockHatsService* mock_hats_service() { return mock_hats_service_; }
  TestingPrefServiceSimple* local_state() { return &local_state_; }

 protected:
  performance_manager::user_tuning::TestUserPerformanceTuningManagerEnvironment
      environment_;

  virtual const std::vector<base::test::FeatureRefAndParams> GetFeatures() {
    return {
        {performance_manager::features::kPerformanceControlsPerformanceSurvey,
         {}},
    };
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<PerformanceControlsHatsService>
      performance_controls_hats_service_;
  raw_ptr<MockHatsService> mock_hats_service_;
};

class PerformanceControlsHatsServiceHasBatteryTest
    : public PerformanceControlsHatsServiceTest {
 public:
  void SetUp() override {
    PerformanceControlsHatsServiceTest::SetUp();

    // Set the battery status so DeviceHasBattery() returns true.
    environment_.battery_level_provider()->SetBatteryState(
        base::test::TestBatteryLevelProvider::CreateBatteryState());
    environment_.sampling_source()->SimulateEvent();

    // Set a recent value for the last battery usage.
    local_state()->SetTime(
        performance_manager::user_tuning::prefs::kLastBatteryUseTimestamp,
        base::Time::Now());
  }

 protected:
  const std::vector<base::test::FeatureRefAndParams> GetFeatures() override {
    return {
        {performance_manager::features::
             kPerformanceControlsBatteryPerformanceSurvey,
         {}},
    };
  }
};

class PerformanceControlsHatsServiceHighEfficiencyOptOutTest
    : public PerformanceControlsHatsServiceTest {
 protected:
  const std::vector<base::test::FeatureRefAndParams> GetFeatures() override {
    return {
        {performance_manager::features::
             kPerformanceControlsHighEfficiencyOptOutSurvey,
         {}},
    };
  }
};

class PerformanceControlsHatsServiceBatterySaverOptOutTest
    : public PerformanceControlsHatsServiceTest {
 protected:
  const std::vector<base::test::FeatureRefAndParams> GetFeatures() override {
    return {
        {performance_manager::features::
             kPerformanceControlsBatterySaverOptOutSurvey,
         {}},
    };
  }
};

TEST_F(PerformanceControlsHatsServiceTest, LaunchesPerformanceSurvey) {
  SetHighEfficiencyEnabled(false);
  SetBatterySaverMode(performance_manager::user_tuning::prefs::
                          BatterySaverModeState::kEnabledBelowThreshold);

  SurveyBitsData expected_bits = {{"high_efficiency_mode", false}};
  SurveyStringData expected_strings = {
      {"battery_saver_mode",
       base::NumberToString(static_cast<int>(
           performance_manager::user_tuning::prefs::BatterySaverModeState::
               kEnabledBelowThreshold))}};
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerPerformanceControlsPerformance, _,
                           _, expected_bits, expected_strings));
  performance_controls_hats_service()->OpenedNewTabPage();
}

TEST_F(PerformanceControlsHatsServiceHasBatteryTest,
       LaunchesBatteryPerformanceSurvey) {
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerPerformanceControlsBatteryPerformance, _,
                   _, _, _));
  performance_controls_hats_service()->OpenedNewTabPage();
}

TEST_F(PerformanceControlsHatsServiceHighEfficiencyOptOutTest,
       LaunchesHighEfficiencyOptOutSurvey) {
  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurvey(
                  kHatsSurveyTriggerPerformanceControlsHighEfficiencyOptOut,
                  10000, _, _));
  SetHighEfficiencyEnabled(false);
}

TEST_F(PerformanceControlsHatsServiceBatterySaverOptOutTest,
       LaunchesBatterySaverOptOutSurvey) {
  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurvey(
                  kHatsSurveyTriggerPerformanceControlsBatterySaverOptOut,
                  10000, _, _));
  SetBatterySaverMode(performance_manager::user_tuning::prefs::
                          BatterySaverModeState::kDisabled);
}
