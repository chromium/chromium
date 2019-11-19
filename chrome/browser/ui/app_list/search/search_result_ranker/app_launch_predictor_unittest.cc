// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_clock_override.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::UnorderedElementsAre;
using testing::Pair;
using testing::FloatEq;

namespace app_list {

namespace {

constexpr char kTarget1[] = "Target1";
constexpr char kTarget2[] = "Target2";

}  // namespace

TEST(AppLaunchPredictorTest, MrfuAppLaunchPredictor) {
  MrfuAppLaunchPredictor predictor;
  const float decay = MrfuAppLaunchPredictor::decay_coeff_;

  predictor.Train(kTarget1);
  const float score_1 = 1.0f - decay;
  EXPECT_THAT(predictor.Rank(),
              UnorderedElementsAre(Pair(kTarget1, FloatEq(score_1))));

  predictor.Train(kTarget1);
  const float score_2 = score_1 + score_1 * decay;
  EXPECT_THAT(predictor.Rank(),
              UnorderedElementsAre(Pair(kTarget1, FloatEq(score_2))));

  predictor.Train(kTarget2);
  const float score_3 = score_2 * decay;
  EXPECT_THAT(predictor.Rank(),
              UnorderedElementsAre(Pair(kTarget1, FloatEq(score_3)),
                                   Pair(kTarget2, FloatEq(score_1))));
}

// Test Serialization logic of SerializedMrfuAppLaunchPredictor.
class SerializedMrfuAppLaunchPredictorTest : public testing::Test {
 protected:
  void SetUp() override {
    score1_ = (1.0f - decay) * decay + (1.0f - decay);
    score2_ = 1.0f - decay;

    auto& predictor_proto =
        *proto_.mutable_serialized_mrfu_app_launch_predictor();

    predictor_proto.set_num_of_trains(3);
    auto& item1 = (*predictor_proto.mutable_scores())[kTarget1];
    item1.set_last_score(score1_);
    item1.set_num_of_trains_at_last_update(2);

    auto& item2 = (*predictor_proto.mutable_scores())[kTarget2];
    item2.set_last_score(score2_);
    item2.set_num_of_trains_at_last_update(3);
  }

  float score1_ = 0.0f;
  float score2_ = 0.0f;
  static constexpr float decay = MrfuAppLaunchPredictor::decay_coeff_;
  AppLaunchPredictorProto proto_;
};

TEST_F(SerializedMrfuAppLaunchPredictorTest, ToProto) {
  SerializedMrfuAppLaunchPredictor predictor;

  predictor.Train(kTarget1);
  predictor.Train(kTarget1);
  predictor.Train(kTarget2);

  // Check predictor.ToProto() is the same as proto_.
  EXPECT_TRUE(EquivToProtoLite(predictor.ToProto(), proto_));
}

TEST_F(SerializedMrfuAppLaunchPredictorTest, FromProto) {
  SerializedMrfuAppLaunchPredictor predictor;
  EXPECT_TRUE(predictor.FromProto(proto_));

  EXPECT_THAT(predictor.Rank(),
              UnorderedElementsAre(Pair(kTarget1, FloatEq(score1_ * decay)),
                                   Pair(kTarget2, FloatEq(score2_))));
}

class HourAppLaunchPredictorTest : public testing::Test {
 protected:
  // Sets local time according to |day_of_week| and |hour_of_day|.
  void SetLocalTime(const int day_of_week, const int hour_of_day) {
    AdvanceToNextLocalSunday();
    const auto advance = base::TimeDelta::FromDays(day_of_week) +
                         base::TimeDelta::FromHours(hour_of_day);
    if (advance > base::TimeDelta()) {
      time_.Advance(advance);
    }
  }

  base::ScopedMockClockOverride time_;

 private:
  // Advances time to be 0am next Sunday.
  void AdvanceToNextLocalSunday() {
    base::Time::Exploded now;
    base::Time::Now().LocalExplode(&now);
    const auto advance = base::TimeDelta::FromDays(6 - now.day_of_week) +
                         base::TimeDelta::FromHours(24 - now.hour);
    if (advance > base::TimeDelta()) {
      time_.Advance(advance);
    }
    base::Time::Now().LocalExplode(&now);
    CHECK_EQ(now.day_of_week, 0);
    CHECK_EQ(now.hour, 0);
  }

};

// Checks HourAppLaunchPredictor::GetBin returns the right bin index for given
// local time.
TEST_F(HourAppLaunchPredictorTest, GetTheRightBin) {
  HourAppLaunchPredictor predictor;

  // Monday.
  for (int i = 0; i <= 23; ++i) {
    SetLocalTime(1, i);
    EXPECT_EQ(predictor.GetBin(), i);
  }

  // Friday.
  for (int i = 0; i <= 23; ++i) {
    SetLocalTime(5, i);
    EXPECT_EQ(predictor.GetBin(), i);
  }

  // Saturday.
  for (int i = 0; i <= 23; ++i) {
    SetLocalTime(6, i);
    EXPECT_EQ(predictor.GetBin(), i + 24);
  }

  // Sunday.
  for (int i = 0; i <= 23; ++i) {
    SetLocalTime(0, i);
    EXPECT_EQ(predictor.GetBin(), i + 24);
  }
}

// Checks the apps are ranked based on frequency in a single bin.
TEST_F(HourAppLaunchPredictorTest, RankFromSingleBin) {
  HourAppLaunchPredictor predictor;
  const auto& weights = HourAppLaunchPredictor::BinWeightsFromFlagOrDefault();

  // Create a model that trained on kTarget1 3 times, and kTarget2 2 times.
  SetLocalTime(1, 10);
  predictor.Train(kTarget1);
  SetLocalTime(2, 10);
  predictor.Train(kTarget1);
  SetLocalTime(3, 10);
  predictor.Train(kTarget1);
  SetLocalTime(4, 10);
  predictor.Train(kTarget2);
  SetLocalTime(5, 10);
  predictor.Train(kTarget2);

  // Train on weekend will not fail into the same bin.
  SetLocalTime(6, 10);
  predictor.Train(kTarget1);
  SetLocalTime(0, 10);
  predictor.Train(kTarget2);

  SetLocalTime(1, 10);
  EXPECT_THAT(predictor.Rank(),
              UnorderedElementsAre(Pair(kTarget1, FloatEq(weights[0] * 0.6)),
                                   Pair(kTarget2, FloatEq(weights[0] * 0.4))));
}

// Checks the apps are ranked based on linearly combined scores from adjacent
// bins.
TEST_F(HourAppLaunchPredictorTest, RankFromMultipleBin) {
  HourAppLaunchPredictor predictor;
  const auto& weights = HourAppLaunchPredictor::BinWeightsFromFlagOrDefault();

  // For bin 10
  SetLocalTime(1, 10);
  predictor.Train(kTarget1);
  predictor.Train(kTarget1);
  SetLocalTime(2, 10);
  predictor.Train(kTarget2);

  // For bin 11
  SetLocalTime(3, 11);
  predictor.Train(kTarget1);
  predictor.Train(kTarget2);

  // FOr bin 12
  SetLocalTime(5, 12);
  predictor.Train(kTarget2);

  // Train on weekend.
  SetLocalTime(6, 10);
  predictor.Train(kTarget1);
  predictor.Train(kTarget2);
  SetLocalTime(0, 11);
  predictor.Train(kTarget2);

  // Check workdays.
  SetLocalTime(1, 10);
  EXPECT_THAT(
      predictor.Rank(),
      UnorderedElementsAre(
          Pair(kTarget1, FloatEq(weights[0] * 2.0 / 3.0 + weights[1] * 0.5)),
          Pair(kTarget2, FloatEq(weights[0] * 1.0 / 3.0 + weights[1] * 0.5 +
                                 weights[2] * 1.0))));

  // Check weekends.
  SetLocalTime(0, 9);
  EXPECT_THAT(
      predictor.Rank(),
      UnorderedElementsAre(
          Pair(kTarget1, FloatEq(weights[1] * 1.0 / 2.0)),
          Pair(kTarget2, FloatEq(weights[1] * 1.0 / 2.0 + weights[2]))));
}

// Check the default weights are set correctly.
TEST_F(HourAppLaunchPredictorTest, CheckDefaultWeights) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      app_list_features::kEnableZeroStateAppsRanker);

  EXPECT_THAT(HourAppLaunchPredictor::BinWeightsFromFlagOrDefault(),
              ElementsAre(FloatEq(0.6), FloatEq(0.15), FloatEq(0.05),
                          FloatEq(0.05), FloatEq(0.15)));
}

// Checks that the weights are set from flag correctly.
TEST_F(HourAppLaunchPredictorTest, SetWeightsFromFlag) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      app_list_features::kEnableZeroStateAppsRanker,
      {{"weight_1_hour_later_bin", "0.1"},
       {"weight_2_hour_later_bin", "0.2"},
       {"weight_2_hour_earlier_bin", "0.22"},
       {"weight_1_hour_earlier_bin", "0.23"}});

  HourAppLaunchPredictor predictor;
  const auto& weights = HourAppLaunchPredictor::BinWeightsFromFlagOrDefault();

  EXPECT_THAT(weights, ElementsAre(FloatEq(0.25), FloatEq(0.1), FloatEq(0.2),
                                   FloatEq(0.22), FloatEq(0.23)));

  // For bin 0
  SetLocalTime(1, 0);
  predictor.Train(kTarget1);
  predictor.Train(kTarget1);
  predictor.Train(kTarget2);

  // For bin 1
  SetLocalTime(1, 1);
  predictor.Train(kTarget1);
  predictor.Train(kTarget2);

  SetLocalTime(1, 0);
  EXPECT_THAT(
      predictor.Rank(),
      UnorderedElementsAre(
          Pair(kTarget1, FloatEq(weights[0] * 2.0 / 3.0 + weights[1] * 0.5)),
          Pair(kTarget2, FloatEq(weights[0] * 1.0 / 3.0 + weights[1] * 0.5))));
}

// Checks FromProto applies decay correctly.
TEST_F(HourAppLaunchPredictorTest, FromProtoDecay) {
  HourAppLaunchPredictor predictor;
  const int bin = predictor.GetBin();
  const int frequency1 = 11;
  const int frequency2 = 1;

  AppLaunchPredictorProto proto;
  auto& frequency_table = (*proto.mutable_hour_app_launch_predictor()
                                ->mutable_binned_frequency_table())[bin];
  (*frequency_table.mutable_frequency())[kTarget1] = frequency1;
  (*frequency_table.mutable_frequency())[kTarget2] = frequency2;
  frequency_table.set_total_counts(frequency1 + frequency2);

  // FromProto will not decay since last_decay_timestamp is not set.
  predictor.FromProto(proto);
  proto.mutable_hour_app_launch_predictor()->set_last_decay_timestamp(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InDays());
  EXPECT_TRUE(EquivToProtoLite(predictor.ToProto(), proto));

  // FromProto will not decay since last_decay_timestamp is within 7 days.
  time_.Advance(base::TimeDelta::FromDays(6));
  predictor.FromProto(proto);
  EXPECT_TRUE(EquivToProtoLite(predictor.ToProto(), proto));

  // FromProto will decay since last_decay_timestamp is over 7 days.
  time_.Advance(base::TimeDelta::FromDays(2));
  predictor.FromProto(proto);
  const int new_frequency1 =
      static_cast<int>(frequency1 * HourAppLaunchPredictor::kWeeklyDecayCoeff);
  frequency_table.mutable_frequency()->clear();
  (*frequency_table.mutable_frequency())[kTarget1] = new_frequency1;
  frequency_table.set_total_counts(new_frequency1);
  proto.mutable_hour_app_launch_predictor()->set_last_decay_timestamp(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InDays());
  EXPECT_TRUE(EquivToProtoLite(predictor.ToProto(), proto));
}

}  // namespace app_list
