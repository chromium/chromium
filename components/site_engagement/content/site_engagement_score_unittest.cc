// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_engagement/content/site_engagement_score.h"

#include <optional>
#include <utility>

#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace site_engagement {

namespace {

const int kLessAccumulationsThanNeededToMaxDailyEngagement = 2;
const int kMoreAccumulationsThanNeededToMaxDailyEngagement = 40;
const int kMoreAccumulationsThanNeededToMaxTotalEngagement = 200;
const int kLessDaysThanNeededToMaxTotalEngagement = 4;
const int kMoreDaysThanNeededToMaxTotalEngagement = 40;
const int kLessPeriodsThanNeededToDecayMaxScore = 2;
const int kMorePeriodsThanNeededToDecayMaxScore = 40;
const double kMaxRoundingDeviation = 0.0001;

base::Time GetReferenceTime() {
  static constexpr base::Time::Exploded kReferenceTime = {.year = 2015,
                                                          .month = 1,
                                                          .day_of_week = 5,
                                                          .day_of_month = 30,
                                                          .hour = 11};
  base::Time out_time;
  EXPECT_TRUE(base::Time::FromLocalExploded(kReferenceTime, &out_time));
  return out_time;
}

}  // namespace

class SiteEngagementScoreTest : public testing::Test {
 public:
  SiteEngagementScoreTest() : score_(&test_clock_, GURL(), nullptr) {}

  void SetUp() override {
    // Disable the first engagement bonus for tests.
    SiteEngagementScore::SetParamValuesForTesting();
  }

 protected:
  void VerifyScore(const SiteEngagementScore& score,
                   double expected_raw_score,
                   double expected_points_added_today,
                   base::Time expected_last_engagement_time) {
    EXPECT_EQ(expected_raw_score, score.raw_score_);
    EXPECT_EQ(expected_points_added_today, score.points_added_today_);
    EXPECT_EQ(expected_last_engagement_time, score.last_engagement_time_);
  }

  void UpdateScore(SiteEngagementScore* score,
                   double raw_score,
                   double points_added_today,
                   base::Time last_engagement_time) {
    score->raw_score_ = raw_score;
    score->points_added_today_ = points_added_today;
    score->last_engagement_time_ = last_engagement_time;
  }

  void TestScoreInitializesAndUpdates(
      base::Value::Dict score_dict,
      double expected_raw_score,
      double expected_points_added_today,
      base::Time expected_last_engagement_time) {
    base::Value::Dict copy(score_dict.Clone());

    SiteEngagementScore initial_score(&test_clock_, GURL(),
                                      std::move(score_dict));
    VerifyScore(initial_score, expected_raw_score, expected_points_added_today,
                expected_last_engagement_time);

    // Updating the score dict should return false, as the score shouldn't
    // have changed at this point.
    EXPECT_FALSE(initial_score.UpdateScoreDict(copy));

    // Update the score to new values and verify it updates the score dict
    // correctly.
    base::Time different_day = GetReferenceTime() + base::Days(1);
    UpdateScore(&initial_score, 5, 10, different_day);
    EXPECT_TRUE(initial_score.UpdateScoreDict(copy));
    SiteEngagementScore updated_score(&test_clock_, GURL(), std::move(copy));
    VerifyScore(updated_score, 5, 10, different_day);
  }

  void SetParamValue(SiteEngagementScore::Variation variation, double value) {
    SiteEngagementScore::GetParamValues()[variation].second = value;
  }

  base::SimpleTestClock test_clock_;
  SiteEngagementScore score_;
};

// Accumulate score many times on the same day. Ensure each time the score goes
// up, but not more than the maximum per day.
TEST_F(SiteEngagementScoreTest, AccumulateOnSameDay) {
  base::Time reference_time = GetReferenceTime();

  test_clock_.SetNow(reference_time);
  for (int i = 0; i < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++i) {
    score_.AddPoints(SiteEngagementScore::GetNavigationPoints());
    EXPECT_EQ(std::min(SiteEngagementScore::GetMaxPointsPerDay(),
                       (i + 1) * SiteEngagementScore::GetNavigationPoints()),
              score_.GetTotalScore());
  }

  EXPECT_EQ(SiteEngagementScore::GetMaxPointsPerDay(), score_.GetTotalScore());
}

// Accumulate on the first day to max that day's engagement, then accumulate on
// a different day.
TEST_F(SiteEngagementScoreTest, AccumulateOnTwoDays) {
  base::Time reference_time = GetReferenceTime();
  base::Time later_date = reference_time + base::Days(2);

  test_clock_.SetNow(reference_time);
  for (int i = 0; i < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++i)
    score_.AddPoints(SiteEngagementScore::GetNavigationPoints());

  EXPECT_EQ(SiteEngagementScore::GetMaxPointsPerDay(), score_.GetTotalScore());

  test_clock_.SetNow(later_date);
  for (int i = 0; i < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++i) {
    score_.AddPoints(SiteEngagementScore::GetNavigationPoints());
    double day_score =
        std::min(SiteEngagementScore::GetMaxPointsPerDay(),
                 (i + 1) * SiteEngagementScore::GetNavigationPoints());
    EXPECT_EQ(day_score + SiteEngagementScore::GetMaxPointsPerDay(),
              score_.GetTotalScore());
  }

  EXPECT_EQ(2 * SiteEngagementScore::GetMaxPointsPerDay(),
            score_.GetTotalScore());
}

// Accumulate score on many consecutive days and ensure the score doesn't exceed
// the maximum allowed.
TEST_F(SiteEngagementScoreTest, AccumulateALotOnManyDays) {
  base::Time current_day = GetReferenceTime();

  for (int i = 0; i < kMoreDaysThanNeededToMaxTotalEngagement; ++i) {
    current_day += base::Days(1);
    test_clock_.SetNow(current_day);
    for (int j = 0; j < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++j)
      score_.AddPoints(SiteEngagementScore::GetNavigationPoints());

    EXPECT_EQ(std::min(SiteEngagementScore::kMaxPoints,
                       (i + 1) * SiteEngagementScore::GetMaxPointsPerDay()),
              score_.GetTotalScore());
  }

  EXPECT_EQ(SiteEngagementScore::kMaxPoints, score_.GetTotalScore());
}

// Accumulate a little on many consecutive days and ensure the score doesn't
// exceed the maximum allowed.
TEST_F(SiteEngagementScoreTest, AccumulateALittleOnManyDays) {
  base::Time current_day = GetReferenceTime();

  for (int i = 0; i < kMoreAccumulationsThanNeededToMaxTotalEngagement; ++i) {
    current_day += base::Days(1);
    test_clock_.SetNow(current_day);

    for (int j = 0; j < kLessAccumulationsThanNeededToMaxDailyEngagement; ++j)
      score_.AddPoints(SiteEngagementScore::GetNavigationPoints());

    EXPECT_EQ(
        std::min(SiteEngagementScore::kMaxPoints,
                 (i + 1) * kLessAccumulationsThanNeededToMaxDailyEngagement *
                     SiteEngagementScore::GetNavigationPoints()),
        score_.GetTotalScore());
  }

  EXPECT_EQ(SiteEngagementScore::kMaxPoints, score_.GetTotalScore());
}

// Accumulate a bit, then check the score decays properly for a range of times.
TEST_F(SiteEngagementScoreTest, ScoresDecayOverTime) {
  base::Time current_day = GetReferenceTime();

  // First max the score.
  for (int i = 0; i < kMoreDaysThanNeededToMaxTotalEngagement; ++i) {
    current_day += base::Days(1);
    test_clock_.SetNow(current_day);

    for (int j = 0; j < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++j)
      score_.AddPoints(SiteEngagementScore::GetNavigationPoints());
  }

  EXPECT_EQ(SiteEngagementScore::kMaxPoints, score_.GetTotalScore());

  // The score should not have decayed before the first decay period has
  // elapsed.
  test_clock_.SetNow(
      current_day +
      base::Hours(SiteEngagementScore::GetDecayPeriodInHours() - 1));
  EXPECT_EQ(SiteEngagementScore::kMaxPoints, score_.GetTotalScore());

  // The score should have decayed by one chunk after one decay period has
  // elapsed.
  test_clock_.SetNow(current_day +
                     base::Hours(SiteEngagementScore::GetDecayPeriodInHours()));
  EXPECT_EQ(
      SiteEngagementScore::kMaxPoints - SiteEngagementScore::GetDecayPoints(),
      score_.GetTotalScore());

  // The score should have decayed by the right number of chunks after a few
  // decay periods have elapsed.
  test_clock_.SetNow(current_day +
                     base::Hours(kLessPeriodsThanNeededToDecayMaxScore *
                                 SiteEngagementScore::GetDecayPeriodInHours()));
  EXPECT_EQ(SiteEngagementScore::kMaxPoints -
                kLessPeriodsThanNeededToDecayMaxScore *
                    SiteEngagementScore::GetDecayPoints(),
            score_.GetTotalScore());

  // The score should not decay below zero.
  test_clock_.SetNow(current_day +
                     base::Hours(kMorePeriodsThanNeededToDecayMaxScore *
                                 SiteEngagementScore::GetDecayPeriodInHours()));
  EXPECT_EQ(0, score_.GetTotalScore());
}

// Test that any expected decays are applied before adding points.
TEST_F(SiteEngagementScoreTest, DecaysAppliedBeforeAdd) {
  base::Time current_day = GetReferenceTime();

  // Get the score up to something that can handle a bit of decay before
  for (int i = 0; i < kLessDaysThanNeededToMaxTotalEngagement; ++i) {
    current_day += base::Days(1);
    test_clock_.SetNow(current_day);

    for (int j = 0; j < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++j)
      score_.AddPoints(SiteEngagementScore::GetNavigationPoints());
  }

  double initial_score = kLessDaysThanNeededToMaxTotalEngagement *
                         SiteEngagementScore::GetMaxPointsPerDay();
  EXPECT_EQ(initial_score, score_.GetTotalScore());

  // Go forward a few decay periods.
  test_clock_.SetNow(current_day +
                     base::Hours(kLessPeriodsThanNeededToDecayMaxScore *
                                 SiteEngagementScore::GetDecayPeriodInHours()));

  double decayed_score =
      initial_score - kLessPeriodsThanNeededToDecayMaxScore *
                          SiteEngagementScore::GetDecayPoints();
  EXPECT_EQ(decayed_score, score_.GetTotalScore());

  // Now add some points.
  score_.AddPoints(SiteEngagementScore::GetNavigationPoints());
  EXPECT_EQ(decayed_score + SiteEngagementScore::GetNavigationPoints(),
            score_.GetTotalScore());
}

// Test that going back in time is handled properly.
TEST_F(SiteEngagementScoreTest, GoBackInTime) {
  base::Time current_day = GetReferenceTime();

  test_clock_.SetNow(current_day);
  for (int i = 0; i < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++i)
    score_.AddPoints(SiteEngagementScore::GetNavigationPoints());

  EXPECT_EQ(SiteEngagementScore::GetMaxPointsPerDay(), score_.GetTotalScore());

  // Adding to the score on an earlier date should be treated like another day,
  // and should not cause any decay.
  test_clock_.SetNow(current_day -
                     base::Days(kMorePeriodsThanNeededToDecayMaxScore *
                                SiteEngagementScore::GetDecayPoints()));
  for (int i = 0; i < kMoreAccumulationsThanNeededToMaxDailyEngagement; ++i) {
    score_.AddPoints(SiteEngagementScore::GetNavigationPoints());
    double day_score =
        std::min(SiteEngagementScore::GetMaxPointsPerDay(),
                 (i + 1) * SiteEngagementScore::GetNavigationPoints());
    EXPECT_EQ(day_score + SiteEngagementScore::GetMaxPointsPerDay(),
              score_.GetTotalScore());
  }

  EXPECT_EQ(2 * SiteEngagementScore::GetMaxPointsPerDay(),
            score_.GetTotalScore());
}

// Test that scores are read / written correctly from / to empty score
// dictionaries.
TEST_F(SiteEngagementScoreTest, EmptyDictionary) {
  TestScoreInitializesAndUpdates(base::Value::Dict(), 0, 0, base::Time());
}

// Test that scores are read / written correctly from / to partially empty
// score dictionaries.
TEST_F(SiteEngagementScoreTest, PartiallyEmptyDictionary) {
  base::Value::Dict dict;
  dict.Set(SiteEngagementScore::kPointsAddedTodayKey, 2.);

  TestScoreInitializesAndUpdates(std::move(dict), 0, 2, base::Time());
}

// Test that scores are read / written correctly from / to populated score
// dictionaries.
TEST_F(SiteEngagementScoreTest, PopulatedDictionary) {
  base::Value::Dict dict;
  dict.Set(SiteEngagementScore::kRawScoreKey, 1.);
  dict.Set(SiteEngagementScore::kPointsAddedTodayKey, 2.);
  dict.Set(SiteEngagementScore::kLastEngagementTimeKey,
           static_cast<double>(GetReferenceTime().ToInternalValue()));

  TestScoreInitializesAndUpdates(std::move(dict), 1, 2, GetReferenceTime());
}

// Ensure bonus engagement is awarded for the first engagement of a day.
TEST_F(SiteEngagementScoreTest, FirstDailyEngagementBonus) {
  SetParamValue(SiteEngagementScore::FIRST_DAILY_ENGAGEMENT, 0.5);

  SiteEngagementScore score1(&test_clock_, GURL(),
                             /*score_dict=*/std::nullopt);
  SiteEngagementScore score2(&test_clock_, GURL(),
                             /*score_dict=*/std::nullopt);
  base::Time current_day = GetReferenceTime();

  test_clock_.SetNow(current_day);

  // The first engagement event gets the bonus.
  score1.AddPoints(0.5);
  EXPECT_EQ(1.0, score1.GetTotalScore());

  // Subsequent events do not.
  score1.AddPoints(0.5);
  EXPECT_EQ(1.5, score1.GetTotalScore());

  // Bonuses are awarded independently between scores.
  score2.AddPoints(1.0);
  EXPECT_EQ(1.5, score2.GetTotalScore());
  score2.AddPoints(1.0);
  EXPECT_EQ(2.5, score2.GetTotalScore());

  test_clock_.SetNow(current_day + base::Days(1));

  // The first event for the next day gets the bonus.
  score1.AddPoints(0.5);
  EXPECT_EQ(2.5, score1.GetTotalScore());

  // Subsequent events do not.
  score1.AddPoints(0.5);
  EXPECT_EQ(3.0, score1.GetTotalScore());

  score2.AddPoints(1.0);
  EXPECT_EQ(4.0, score2.GetTotalScore());
  score2.AddPoints(1.0);
  EXPECT_EQ(5.0, score2.GetTotalScore());
}

// Test that resetting a score has the correct properties.
TEST_F(SiteEngagementScoreTest, Reset) {
  base::Time current_day = GetReferenceTime();

  test_clock_.SetNow(current_day);
  score_.AddPoints(SiteEngagementScore::GetNavigationPoints());
  EXPECT_EQ(SiteEngagementScore::GetNavigationPoints(), score_.GetTotalScore());

  current_day += base::Days(7);
  test_clock_.SetNow(current_day);

  score_.Reset(20.0, current_day);
  EXPECT_DOUBLE_EQ(20.0, score_.GetTotalScore());
  EXPECT_DOUBLE_EQ(0, score_.points_added_today_);
  EXPECT_EQ(current_day, score_.last_engagement_time_);
  EXPECT_TRUE(score_.last_shortcut_launch_time_.is_null());

  // Adding points after the reset should work as normal.
  score_.AddPoints(5);
  EXPECT_EQ(25.0, score_.GetTotalScore());

  // The decay should happen one decay period from the current time.
  test_clock_.SetNow(
      current_day +
      base::Hours(SiteEngagementScore::GetDecayPeriodInHours() + 1));
  EXPECT_EQ(25.0 - SiteEngagementScore::GetDecayPoints(),
            score_.GetTotalScore());

  // Ensure that manually setting a time works as expected.
  score_.AddPoints(5);
  test_clock_.SetNow(GetReferenceTime());
  base::Time now = test_clock_.Now();
  score_.Reset(10.0, now);

  EXPECT_DOUBLE_EQ(10.0, score_.GetTotalScore());
  EXPECT_DOUBLE_EQ(0, score_.points_added_today_);
  EXPECT_EQ(now, score_.last_engagement_time_);
  EXPECT_TRUE(score_.last_shortcut_launch_time_.is_null());

  base::Time old_now = test_clock_.Now();

  score_.set_last_shortcut_launch_time(test_clock_.Now());
  test_clock_.SetNow(GetReferenceTime() + base::Days(3));
  now = test_clock_.Now();
  score_.Reset(15.0, now);

  // 5 bonus from the last shortcut launch.
  EXPECT_DOUBLE_EQ(20.0, score_.GetTotalScore());
  EXPECT_DOUBLE_EQ(0, score_.points_added_today_);
  EXPECT_EQ(now, score_.last_engagement_time_);
  EXPECT_EQ(old_now, score_.last_shortcut_launch_time_);
}

// Test proportional decay.
TEST_F(SiteEngagementScoreTest, ProportionalDecay) {
  SetParamValue(SiteEngagementScore::DECAY_PROPORTION, 0.5);
  SetParamValue(SiteEngagementScore::DECAY_POINTS, 0);
  SetParamValue(SiteEngagementScore::MAX_POINTS_PER_DAY, 20);
  base::Time current_day = GetReferenceTime();
  test_clock_.SetNow(current_day);

  // Single decay period, expect the score to be halved once.
  score_.AddPoints(2.0);
  current_day += base::Days(7);
  test_clock_.SetNow(current_day);
  EXPECT_DOUBLE_EQ(1.0, score_.GetTotalScore());

  // 3 decay periods, expect the score to be halved 3 times.
  score_.AddPoints(15.0);
  current_day += base::Days(21);
  test_clock_.SetNow(current_day);
  EXPECT_DOUBLE_EQ(2.0, score_.GetTotalScore());

  // Ensure point removal happens after proportional decay.
  score_.AddPoints(4.0);
  EXPECT_DOUBLE_EQ(6.0, score_.GetTotalScore());
  SetParamValue(SiteEngagementScore::DECAY_POINTS, 2.0);
  current_day += base::Days(7);
  test_clock_.SetNow(current_day);
  EXPECT_NEAR(1.0, score_.GetTotalScore(), kMaxRoundingDeviation);
}

// Verify that GetDetails fills out all fields correctly.
TEST_F(SiteEngagementScoreTest, GetDetails) {
  // Advance the clock, otherwise Now() is the same as the null Time value.
  test_clock_.Advance(base::Days(365));

  GURL url("http://www.google.com/");

  // Replace |score_| with one with an actual URL.
  score_ = SiteEngagementScore(&test_clock_, url, nullptr);

  // Initially all component scores should be zero.
  mojom::SiteEngagementDetails details = score_.GetDetails();
  EXPECT_DOUBLE_EQ(0.0, details.total_score);
  EXPECT_DOUBLE_EQ(0.0, details.installed_bonus);
  EXPECT_DOUBLE_EQ(0.0, details.base_score);
  EXPECT_EQ(url, details.origin);

  // Simulate the app having been launched.
  score_.set_last_shortcut_launch_time(test_clock_.Now());
  details = score_.GetDetails();
  EXPECT_DOUBLE_EQ(details.installed_bonus, details.total_score);
  EXPECT_LT(0.0, details.installed_bonus);
  EXPECT_DOUBLE_EQ(0.0, details.base_score);
}

}  // namespace site_engagement
