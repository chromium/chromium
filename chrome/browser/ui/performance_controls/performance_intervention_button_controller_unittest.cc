// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller.h"

#include <memory>

#include "base/check_op.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/feature_engagement/test/test_tracker.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class PerformanceInterventionButtonControllerUnitTest : public testing::Test {
 public:
  void SetUp() override {
    std::map<std::string, std::string> params;
    params["availability"] = "any";
    params["session_rate"] = "any";
    params["event_used"] = "name:;comparator:any;window:0;storage:360";
    params["event_trigger"] =
        "name:performance_intervention_dialog_trigger;comparator:<5;window:1;"
        "storage:360";
    params["event_weekly"] =
        "name:performance_intervention_dialog_trigger;comparator:<35;window:7;"
        "storage:360";

    feature_list_.InitAndEnableFeaturesWithParameters(
        {{performance_manager::features::
              kPerformanceInterventionNotificationImprovements,
          {}},
         {feature_engagement::kIPHPerformanceInterventionDialogFeature,
          params}});

    tracker_ = feature_engagement::CreateTestTracker();
    base::RunLoop run_loop;
    tracker()->AddOnInitializedCallback(base::BindOnce(
        [](base::OnceClosure callback, bool success) {
          ASSERT_TRUE(success);
          std::move(callback).Run();
        },
        run_loop.QuitClosure()));
    run_loop.Run();

    controller_ = std::make_unique<PerformanceInterventionButtonController>(
        nullptr, nullptr);
  }

  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

  PerformanceInterventionButtonController* controller() {
    return controller_.get();
  }

  feature_engagement::Tracker* tracker() { return tracker_.get(); }

  // Simulates triggering the intervention and recording it being shown with the
  // feature engagement tracker.
  bool SimulateTriggeringIntervention() {
    feature_engagement::Tracker* const feature_engagement_tracker = tracker();
    const bool should_show =
        controller()->ShouldShowNotification(feature_engagement_tracker);

    if (should_show) {
      CHECK(feature_engagement_tracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHPerformanceInterventionDialogFeature));
      feature_engagement_tracker->Dismissed(
          feature_engagement::kIPHPerformanceInterventionDialogFeature);
      g_browser_process->local_state()->SetTime(
          performance_manager::user_tuning::prefs::
              kPerformanceInterventionNotificationLastShown,
          base::Time::Now());
    }
    return should_show;
  }

  // Populate the intervention accept history so that only the most recent
  // `num_accepts` is recorded as accepted in history.
  void PopulateAcceptHistory(int num_accepts) {
    const int max_accept =
        performance_manager::features::kAcceptanceRateWindowSize.Get();
    CHECK_LE(num_accepts, max_accept);
    PrefService* const pref_service = g_browser_process->local_state();

    base::Value::List previous_acceptance = base::Value::List();
    for (int i = 0; i < max_accept - num_accepts; i++) {
      previous_acceptance.Append(false);
    }

    for (int i = 0; i < num_accepts; i++) {
      previous_acceptance.Append(true);
    }

    pref_service->SetList(performance_manager::user_tuning::prefs::
                              kPerformanceInterventionNotificationAcceptHistory,
                          std::move(previous_acceptance));
  }

 private:
  feature_engagement::test::ScopedIphFeatureList feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedTestingLocalState scoped_testing_local_state_{
      TestingBrowserProcess::GetGlobal()};
  std::unique_ptr<PerformanceInterventionButtonController> controller_;
  std::unique_ptr<feature_engagement::Tracker> tracker_;
};

TEST_F(PerformanceInterventionButtonControllerUnitTest,
       EnsureTimeBetweenIntervention) {
  EXPECT_TRUE(SimulateTriggeringIntervention());

  // Intervention should not show because the minimum time between reshow
  // did not elapse yet.
  EXPECT_FALSE(SimulateTriggeringIntervention());

  // The intervention should show after the minimum time has elapsed.
  task_environment().FastForwardBy(
      performance_manager::features::kMinimumTimeBetweenReshow.Get());
  EXPECT_TRUE(SimulateTriggeringIntervention());
}

TEST_F(PerformanceInterventionButtonControllerUnitTest,
       FluctuatingDailyAcceptance) {
  // Pre-populate the accept history to 20% so that the intervention should
  // only show at most once per day.
  PopulateAcceptHistory(2);
  EXPECT_TRUE(SimulateTriggeringIntervention());

  // The intervention should not show after the minimum reshow duration has
  // passed because the intervention has already shown for the day and meets the
  // calculated limited.
  task_environment().FastForwardBy(
      performance_manager::features::kMinimumTimeBetweenReshow.Get());
  EXPECT_FALSE(SimulateTriggeringIntervention());

  // The intervention should be allowed to show after a day passes.
  task_environment().FastForwardBy(base::Days(1));
  EXPECT_TRUE(SimulateTriggeringIntervention());

  // Adjust the acceptance rate to 30% so that the intervention should be
  // eligible to show twice per day now.
  PopulateAcceptHistory(3);
  task_environment().FastForwardBy(
      performance_manager::features::kMinimumTimeBetweenReshow.Get());
  EXPECT_TRUE(SimulateTriggeringIntervention());
}

TEST_F(PerformanceInterventionButtonControllerUnitTest,
       FluctuatingWeeklyAcceptance) {
  // Pre-populate the acceptance rate to 100% so the intervention can show at
  // most 5 times per day and 35 times per week.
  PopulateAcceptHistory(10);
  for (int i = 0; i < 3; i++) {
    EXPECT_TRUE(SimulateTriggeringIntervention());
    task_environment().FastForwardBy(
        performance_manager::features::kMinimumTimeBetweenReshow.Get());
  }

  // Adjust the acceptance rate to 10% so the intervention can only show at most
  // once per day and 3 times per week.
  task_environment().FastForwardBy(base::Days(1));
  PopulateAcceptHistory(1);
  // The intervention should not show even though it is a new day because the
  // intervention has shown more than the calculated weekly limit.
  EXPECT_FALSE(SimulateTriggeringIntervention());

  // Intervention should show after a week passes.
  task_environment().FastForwardBy(base::Days(7));
  EXPECT_TRUE(SimulateTriggeringIntervention());
}

TEST_F(PerformanceInterventionButtonControllerUnitTest, ZeroAcceptanceRate) {
  // The intervention should show even with a 0% acceptance rate.
  PopulateAcceptHistory(0);
  EXPECT_TRUE(SimulateTriggeringIntervention());

  // Since the acceptance rate is still 0, the intervention should not be
  // allowed to show until a month has passed since the last shown time.
  task_environment().FastForwardBy(base::Days(7));
  EXPECT_FALSE(SimulateTriggeringIntervention());

  task_environment().FastForwardBy(
      performance_manager::features::kNoAcceptanceBackOff.Get());
  EXPECT_TRUE(SimulateTriggeringIntervention());
}

TEST_F(PerformanceInterventionButtonControllerUnitTest,
       AcceptHistoryExceedAcceptanceWindow) {
  // Pre-populate the accept history before the
  // PerformanceInterventionButtonController is created to simulate establishing
  // a large acceptance history with an old acceptance window size from a
  // previous user session.
  const int max_accept =
      performance_manager::features::kAcceptanceRateWindowSize.Get();
  PrefService* const pref_service = g_browser_process->local_state();
  base::Value::List previous_acceptance = base::Value::List();
  for (int i = 0; i < max_accept; i++) {
    previous_acceptance.Append(false);
  }
  for (int i = 0; i < max_accept / 2; i++) {
    previous_acceptance.Append(true);
  }
  pref_service->SetList(performance_manager::user_tuning::prefs::
                            kPerformanceInterventionNotificationAcceptHistory,
                        std::move(previous_acceptance));
  // Even though the the list exceeds the window size, only the most recent
  // entries within the window should be considered when determining acceptance
  // percentage.
  EXPECT_EQ(50,
            PerformanceInterventionButtonController::GetAcceptancePercentage());

  // The acceptance history should be corrected to use the correct window size
  // upon construction.
  auto button_controller =
      std::make_unique<PerformanceInterventionButtonController>(nullptr,
                                                                nullptr);
  EXPECT_EQ(50,
            PerformanceInterventionButtonController::GetAcceptancePercentage());
  EXPECT_EQ(static_cast<size_t>(max_accept),
            pref_service
                ->GetList(performance_manager::user_tuning::prefs::
                              kPerformanceInterventionNotificationAcceptHistory)
                .size());
}
