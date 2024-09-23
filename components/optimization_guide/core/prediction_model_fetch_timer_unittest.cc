// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/prediction_model_fetch_timer.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "prediction_model_fetch_timer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class PredictionModelFetchTimerTestBase : public testing::Test {
 public:
  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    prefs::RegisterProfilePrefs(pref_service_->registry());
    prediction_model_fetch_timer_ = std::make_unique<PredictionModelFetchTimer>(
        pref_service_.get(),
        base::BindRepeating(&PredictionModelFetchTimerTestBase::OnFetchModels,
                            base::Unretained(this)));
    prediction_model_fetch_timer_->SetClockForTesting(
        task_environment_.GetMockClock());
  }

  void MoveClockTillFirstModelFetch() {
    MoveClockForwardBy(features::PredictionModelFetchStartupDelay());
  }

  void MoveClockForwardBy(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
    RunUntilIdle();
  }

  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  PredictionModelFetchTimer::PredictionModelFetchTimerState
  GetPredictionModelFetchTimerState() const {
    return prediction_model_fetch_timer_->GetStateForTesting();
  }

 protected:
  // Called when the fetch timer is fired.
  void OnFetchModels() {
    last_model_fetch_time_ = task_environment_.GetMockClock()->Now();
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<PredictionModelFetchTimer> prediction_model_fetch_timer_;

  // Holds the last time the models were fetched.
  std::optional<base::Time> last_model_fetch_time_;
};

class PredictionModelFetchTimerTest : public PredictionModelFetchTimerTestBase {
};

TEST_F(PredictionModelFetchTimerTest, FirstModelFetch) {
  prediction_model_fetch_timer_->MaybeScheduleFirstModelFetch();
  MoveClockTillFirstModelFetch();
  EXPECT_TRUE(last_model_fetch_time_);
  EXPECT_EQ(PredictionModelFetchTimer::kFirstFetch,
            GetPredictionModelFetchTimerState());
}

TEST_F(PredictionModelFetchTimerTest, FirstModelFetchSuccess) {
  prediction_model_fetch_timer_->MaybeScheduleFirstModelFetch();
  MoveClockTillFirstModelFetch();
  EXPECT_TRUE(last_model_fetch_time_);
  EXPECT_EQ(PredictionModelFetchTimer::kFirstFetch,
            GetPredictionModelFetchTimerState());
  last_model_fetch_time_ = std::nullopt;

  prediction_model_fetch_timer_->NotifyModelFetchAttempt();
  prediction_model_fetch_timer_->NotifyModelFetchSuccess();
  prediction_model_fetch_timer_->SchedulePeriodicModelsFetch();
  EXPECT_EQ(features::PredictionModelFetchInterval(),
            prediction_model_fetch_timer_->GetFetchTimerForTesting()
                ->GetCurrentDelay());
  MoveClockForwardBy(features::PredictionModelFetchInterval() +
                     features::PredictionModelFetchRandomMaxDelay());
  EXPECT_TRUE(last_model_fetch_time_);
  EXPECT_EQ(PredictionModelFetchTimer::kPeriodicFetch,
            GetPredictionModelFetchTimerState());
}

TEST_F(PredictionModelFetchTimerTest, FirstModelFetchFailure) {
  prediction_model_fetch_timer_->NotifyModelFetchAttempt();
  prediction_model_fetch_timer_->NotifyModelFetchSuccess();
  MoveClockForwardBy(features::PredictionModelFetchInterval() +
                     features::PredictionModelFetchRandomMaxDelay());

  prediction_model_fetch_timer_->MaybeScheduleFirstModelFetch();
  MoveClockTillFirstModelFetch();
  EXPECT_TRUE(last_model_fetch_time_);
  EXPECT_EQ(PredictionModelFetchTimer::kFirstFetch,
            GetPredictionModelFetchTimerState());
  last_model_fetch_time_ = std::nullopt;

  // Without updating the model fetch attempt, it will retry.
  prediction_model_fetch_timer_->SchedulePeriodicModelsFetch();
  auto delay = prediction_model_fetch_timer_->GetFetchTimerForTesting()
                   ->GetCurrentDelay();
  EXPECT_LE(features::PredictionModelFetchRandomMinDelay(), delay);
  EXPECT_GE(features::PredictionModelFetchRandomMaxDelay(), delay);
  MoveClockForwardBy(features::PredictionModelFetchRandomMaxDelay());
  EXPECT_TRUE(last_model_fetch_time_);

  // Without updating the model fetch success attempt, it will retry.
  last_model_fetch_time_ = std::nullopt;
  prediction_model_fetch_timer_->NotifyModelFetchAttempt();
  prediction_model_fetch_timer_->SchedulePeriodicModelsFetch();
  EXPECT_EQ(features::PredictionModelFetchRetryDelay(),
            prediction_model_fetch_timer_->GetFetchTimerForTesting()
                ->GetCurrentDelay());
  MoveClockForwardBy(features::PredictionModelFetchRetryDelay() +
                     features::PredictionModelFetchRandomMaxDelay());
  EXPECT_TRUE(last_model_fetch_time_);

  // With model fetch attempt and success updated, it will be periodic fetch.
  last_model_fetch_time_ = std::nullopt;
  prediction_model_fetch_timer_->NotifyModelFetchAttempt();
  prediction_model_fetch_timer_->NotifyModelFetchSuccess();
  prediction_model_fetch_timer_->SchedulePeriodicModelsFetch();
  EXPECT_EQ(features::PredictionModelFetchInterval(),
            prediction_model_fetch_timer_->GetFetchTimerForTesting()
                ->GetCurrentDelay());
  MoveClockForwardBy(features::PredictionModelFetchInterval() +
                     features::PredictionModelFetchRandomMaxDelay());
  EXPECT_TRUE(last_model_fetch_time_);
  EXPECT_EQ(PredictionModelFetchTimer::kPeriodicFetch,
            GetPredictionModelFetchTimerState());
}

TEST_F(PredictionModelFetchTimerTest, NewRegistrationFetchEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationTargetPrediction,
      {{"new_registration_fetch_min_delay", "5s"},
       {"new_registration_fetch_max_delay", "10s"}});
  prediction_model_fetch_timer_->MaybeScheduleFirstModelFetch();
  RunUntilIdle();

  // Any new model registrations before the first fetch should have no effect.
  prediction_model_fetch_timer_->ScheduleFetchOnModelRegistration();
  EXPECT_EQ(PredictionModelFetchTimer::kFirstFetch,
            GetPredictionModelFetchTimerState());

  MoveClockTillFirstModelFetch();
  EXPECT_TRUE(last_model_fetch_time_);
  last_model_fetch_time_ = std::nullopt;

  // Complete the first model fetch.
  prediction_model_fetch_timer_->NotifyModelFetchAttempt();
  prediction_model_fetch_timer_->NotifyModelFetchSuccess();
  prediction_model_fetch_timer_->SchedulePeriodicModelsFetch();
  EXPECT_EQ(PredictionModelFetchTimer::kPeriodicFetch,
            GetPredictionModelFetchTimerState());
  EXPECT_EQ(features::PredictionModelFetchInterval(),
            prediction_model_fetch_timer_->GetFetchTimerForTesting()
                ->GetCurrentDelay());

  // New model registrations will trigger the model fetch again.
  last_model_fetch_time_ = std::nullopt;
  prediction_model_fetch_timer_->ScheduleFetchOnModelRegistration();
  EXPECT_EQ(PredictionModelFetchTimer::kNewRegistrationFetch,
            GetPredictionModelFetchTimerState());
  auto delay = prediction_model_fetch_timer_->GetFetchTimerForTesting()
                   ->GetCurrentDelay();
  EXPECT_LE(base::Seconds(5), delay);
  EXPECT_GE(base::Seconds(10), delay);
  MoveClockForwardBy(base::Seconds(10));
  EXPECT_TRUE(last_model_fetch_time_);
  last_model_fetch_time_ = std::nullopt;

  // Subsequently it will be periodic fetch.
  prediction_model_fetch_timer_->NotifyModelFetchAttempt();
  prediction_model_fetch_timer_->NotifyModelFetchSuccess();
  prediction_model_fetch_timer_->SchedulePeriodicModelsFetch();
  EXPECT_EQ(PredictionModelFetchTimer::kPeriodicFetch,
            GetPredictionModelFetchTimerState());
  EXPECT_EQ(features::PredictionModelFetchInterval(),
            prediction_model_fetch_timer_->GetFetchTimerForTesting()
                ->GetCurrentDelay());
}

}  // namespace optimization_guide
