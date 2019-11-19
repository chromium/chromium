// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"

#include <exception>
#include <map>
#include <memory>
#include <vector>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash/hash.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor_test_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_config.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::FloatEq;
using testing::FloatNear;
using testing::Pair;
using testing::UnorderedElementsAre;

namespace app_list {
namespace {

const uint32_t kCondition = 0u;

}

class FrecencyPredictorTest : public testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();

    config_.set_decay_coeff(0.5f);
    predictor_ = std::make_unique<FrecencyPredictor>(config_, "model");
  }

  FrecencyPredictorConfig config_;
  std::unique_ptr<FrecencyPredictor> predictor_;
};

TEST_F(FrecencyPredictorTest, RankWithNoTargets) {
  EXPECT_TRUE(predictor_->Rank(kCondition).empty());
}

TEST_F(FrecencyPredictorTest, RecordAndRankSimple) {
  predictor_->Train(2u, kCondition);
  predictor_->Train(4u, kCondition);
  predictor_->Train(6u, kCondition);

  const float total = 0.5f + 0.25f + 0.125f;
  EXPECT_THAT(predictor_->Rank(kCondition),
              UnorderedElementsAre(Pair(2u, FloatEq(0.125f / total)),
                                   Pair(4u, FloatEq(0.25f / total)),
                                   Pair(6u, FloatEq(0.5f / total))));
}

TEST_F(FrecencyPredictorTest, RecordAndRankComplex) {
  predictor_->Train(2u, kCondition);
  predictor_->Train(4u, kCondition);
  predictor_->Train(6u, kCondition);
  predictor_->Train(4u, kCondition);
  predictor_->Train(2u, kCondition);

  // Ranks should be deterministic.
  const float total = 0.53125f + 0.3125f + 0.125f;
  for (int i = 0; i < 3; ++i) {
    EXPECT_THAT(predictor_->Rank(kCondition),
                UnorderedElementsAre(Pair(2u, FloatEq(0.53125f / total)),
                                     Pair(4u, FloatEq(0.3125f / total)),
                                     Pair(6u, FloatEq(0.125f / total))));
  }
}

TEST_F(FrecencyPredictorTest, Cleanup) {
  for (int i = 0; i < 6; ++i)
    predictor_->Train(i, kCondition);
  predictor_->Cleanup({0u, 2u, 4u});

  EXPECT_THAT(predictor_->Rank(kCondition),
              UnorderedElementsAre(Pair(0u, _), Pair(2u, _), Pair(4u, _)));
}

TEST_F(FrecencyPredictorTest, ToAndFromProto) {
  predictor_->Train(1u, kCondition);
  predictor_->Train(3u, kCondition);
  predictor_->Train(5u, kCondition);

  RecurrencePredictorProto proto;
  predictor_->ToProto(&proto);

  FrecencyPredictor new_predictor(config_, "model");
  new_predictor.FromProto(proto);

  EXPECT_TRUE(proto.has_frecency_predictor());
  EXPECT_EQ(proto.frecency_predictor().num_updates(), 3u);
  EXPECT_EQ(predictor_->Rank(kCondition), new_predictor.Rank(kCondition));
}

class ConditionalFrequencyPredictorTest : public testing::Test {};

TEST_F(ConditionalFrequencyPredictorTest, TrainAndRank) {
  ConditionalFrequencyPredictor cfp("model");

  cfp.TrainWithDelta(0u, 1u, 5.0f);
  cfp.TrainWithDelta(1u, 1u, 1.0f);
  cfp.TrainWithDelta(1u, 1u, 1.0f);
  cfp.TrainWithDelta(1u, 50u, 1.0f);
  cfp.TrainWithDelta(1u, 50u, 2.0f);
  cfp.TrainWithDelta(2u, 50u, 1.0f);
  cfp.TrainWithDelta(2u, 50u, 1.0f);

  EXPECT_THAT(cfp.Rank(1u),
              UnorderedElementsAre(Pair(0u, FloatEq(5.0f / 7.0f)),
                                   Pair(1u, FloatEq(2.0f / 7.0f))));
  EXPECT_THAT(cfp.Rank(50u),
              UnorderedElementsAre(Pair(1u, FloatEq(3.0f / 5.0f)),
                                   Pair(2u, FloatEq(2.0f / 5.0f))));
}

TEST_F(ConditionalFrequencyPredictorTest, Cleanup) {
  ConditionalFrequencyPredictor cfp("model");

  cfp.Train(0u, 0u);
  for (int i = 0; i < 6; ++i) {
    cfp.Train(i, 0u);
    cfp.Train(2 * i, 1u);
    cfp.Train(2 * i + 1, 2u);
  }
  cfp.Cleanup({0u, 2u, 4u});

  EXPECT_THAT(cfp.Rank(0u), UnorderedElementsAre(Pair(0u, FloatEq(0.5f)),
                                                 Pair(2u, FloatEq(0.25f)),
                                                 Pair(4u, FloatEq(0.25f))));
  EXPECT_THAT(cfp.Rank(1u),
              UnorderedElementsAre(Pair(0u, FloatEq(1.0f / 3.0f)),
                                   Pair(2u, FloatEq(1.0f / 3.0f)),
                                   Pair(4u, FloatEq(1.0f / 3.0f))));
  EXPECT_TRUE(cfp.Rank(2u).empty());
}

TEST_F(ConditionalFrequencyPredictorTest, ToFromProto) {
  ConditionalFrequencyPredictor cfp1("model");

  cfp1.Train(1u, 1u);
  cfp1.Train(2u, 1u);
  cfp1.Train(3u, 1u);
  cfp1.Train(4u, 1u);
  cfp1.Train(1u, 2u);
  cfp1.Train(1u, 2u);
  cfp1.TrainWithDelta(2u, 3u, 3.0f);
  cfp1.Train(3u, 3u);

  RecurrencePredictorProto proto;
  cfp1.ToProto(&proto);

  ConditionalFrequencyPredictor cfp2("model");
  cfp2.FromProto(proto);

  EXPECT_THAT(
      cfp2.Rank(1u),
      UnorderedElementsAre(Pair(1u, FloatEq(0.25f)), Pair(2u, FloatEq(0.25f)),
                           Pair(3u, FloatEq(0.25f)), Pair(4u, FloatEq(0.25f))));
  EXPECT_THAT(cfp2.Rank(2u), UnorderedElementsAre(Pair(1u, FloatEq(1.0f))));
  EXPECT_THAT(cfp2.Rank(3u), UnorderedElementsAre(Pair(2u, FloatEq(0.75f)),
                                                  Pair(3u, FloatEq(0.25f))));
}

class HourBinPredictorTest : public testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();

    config_.set_weekly_decay_coeff(0.5f);

    const std::map<int, float> bin_weights = {
        {-2, 0.05}, {-1, 0.15}, {0, 0.6}, {1, 0.15}, {2, 0.05}};
    for (const auto& pair : bin_weights) {
      auto* config_pair = config_.add_bin_weights();
      config_pair->set_bin(pair.first);
      config_pair->set_weight(pair.second);
    }

    predictor_ = std::make_unique<HourBinPredictor>(config_, "model");
  }

  // Sets local time according to |day_of_week| and |hour_of_day|.
  void SetLocalTime(const int day_of_week, const int hour_of_day) {
    AdvanceToNextLocalSunday();
    const auto advance = base::TimeDelta::FromDays(day_of_week) +
                         base::TimeDelta::FromHours(hour_of_day);
    if (advance > base::TimeDelta()) {
      time_.Advance(advance);
    }
  }

  RecurrencePredictorProto MakeTestingHourBinnedProto() {
    RecurrencePredictorProto proto;
    auto* hour_bin_proto = proto.mutable_hour_bin_predictor();
    hour_bin_proto->set_last_decay_timestamp(365);

    HourBinPredictorProto::FrequencyTable frequency_table;
    (*frequency_table.mutable_frequency())[1u] = 3;
    (*frequency_table.mutable_frequency())[2u] = 1;
    frequency_table.set_total_counts(4);
    (*hour_bin_proto->mutable_binned_frequency_table())[10] = frequency_table;

    frequency_table = HourBinPredictorProto::FrequencyTable();
    (*frequency_table.mutable_frequency())[1u] = 1;
    (*frequency_table.mutable_frequency())[3u] = 1;
    frequency_table.set_total_counts(2);
    (*hour_bin_proto->mutable_binned_frequency_table())[11] = frequency_table;

    return proto;
  }

  base::ScopedMockClockOverride time_;
  HourBinPredictorConfig config_;
  std::unique_ptr<HourBinPredictor> predictor_;

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

TEST_F(HourBinPredictorTest, RankWithNoTargets) {
  EXPECT_TRUE(predictor_->Rank(0u).empty());
}

TEST_F(HourBinPredictorTest, GetTheRightBin) {
  // Monday.
  for (int i = 0; i <= 23; ++i) {
    SetLocalTime(1, i);
    EXPECT_EQ(predictor_->GetBin(), i);
  }

  // Friday.
  for (int i = 0; i <= 23; ++i) {
    SetLocalTime(5, i);
    EXPECT_EQ(predictor_->GetBin(), i);
  }

  // Saturday.
  for (int i = 0; i <= 23; ++i) {
    SetLocalTime(6, i);
    EXPECT_EQ(predictor_->GetBin(), i + 24);
  }

  // Sunday.
  for (int i = 0; i <= 23; ++i) {
    SetLocalTime(0, i);
    EXPECT_EQ(predictor_->GetBin(), i + 24);
  }

  // 2 hour before 00:00 Monday is 22:00 Sunday
  SetLocalTime(1, 0);
  EXPECT_EQ(predictor_->GetBinFromHourDifference(-2), 22 + 24);

  // 3 hour after 23:00 Friday is 02:00 Saturday
  SetLocalTime(5, 23);
  EXPECT_EQ(predictor_->GetBinFromHourDifference(3), 2 + 24);

  // 4 hour after 22:00 Sunday is 2:00 Monday
  SetLocalTime(0, 22);
  EXPECT_EQ(predictor_->GetBinFromHourDifference(4), 2);

  // 5 hour before 3:00 Saturday is 22:00 Friday
  SetLocalTime(6, 3);
  EXPECT_EQ(predictor_->GetBinFromHourDifference(-5), 22);
}

TEST_F(HourBinPredictorTest, TrainAndRankSingleBin) {
  std::map<int, float> weights;
  for (const auto& pair : config_.bin_weights())
    weights[pair.bin()] = pair.weight();

  SetLocalTime(1, 10);
  predictor_->Train(1u, kCondition);
  SetLocalTime(2, 10);
  predictor_->Train(1u, kCondition);
  SetLocalTime(3, 10);
  predictor_->Train(2u, kCondition);
  SetLocalTime(4, 10);
  predictor_->Train(1u, kCondition);
  SetLocalTime(5, 10);
  predictor_->Train(2u, kCondition);

  // Train on weekend doesn't affect the result during the week
  SetLocalTime(0, 10);
  predictor_->Train(1u, kCondition);
  SetLocalTime(0, 10);
  predictor_->Train(2u, kCondition);

  SetLocalTime(1, 10);
  EXPECT_THAT(predictor_->Rank(kCondition),
              UnorderedElementsAre(Pair(1u, FloatEq(weights[0] * 0.6)),
                                   Pair(2u, FloatEq(weights[0] * 0.4))));
}

TEST_F(HourBinPredictorTest, TrainAndRankMultipleBin) {
  std::map<int, float> weights;
  for (const auto& pair : config_.bin_weights())
    weights[pair.bin()] = pair.weight();

  // For bin 10
  SetLocalTime(1, 10);
  predictor_->Train(1u, kCondition);
  predictor_->Train(1u, kCondition);
  SetLocalTime(2, 10);
  predictor_->Train(2u, kCondition);

  // For bin 11
  SetLocalTime(3, 11);
  predictor_->Train(1u, kCondition);
  predictor_->Train(2u, kCondition);
  // For bin 12
  SetLocalTime(5, 12);
  predictor_->Train(2u, kCondition);

  // Train on weekend.
  SetLocalTime(6, 10);
  predictor_->Train(1u, kCondition);
  predictor_->Train(2u, kCondition);
  SetLocalTime(0, 11);
  predictor_->Train(2u, kCondition);

  // Check workdays.
  SetLocalTime(1, 10);
  EXPECT_THAT(
      predictor_->Rank(kCondition),
      UnorderedElementsAre(
          Pair(1u, FloatEq((weights)[0] * 2.0 / 3.0 + weights[1] * 0.5)),
          Pair(2u, FloatEq(weights[0] * 1.0 / 3.0 + weights[1] * 0.5 +
                           weights[2] * 1.0))));

  // Check weekends.
  SetLocalTime(0, 9);
  EXPECT_THAT(predictor_->Rank(kCondition),
              UnorderedElementsAre(
                  Pair(1u, FloatEq(weights[1] * 1.0 / 2.0)),
                  Pair(2u, FloatEq(weights[1] * 1.0 / 2.0 + weights[2]))));
}

TEST_F(HourBinPredictorTest, FromProto) {
  RecurrencePredictorProto proto = MakeTestingHourBinnedProto();
  predictor_->FromProto(proto);
  SetLocalTime(1, 11);
  EXPECT_THAT(
      predictor_->Rank(kCondition),
      UnorderedElementsAre(Pair(1u, FloatEq(0.4125)), Pair(2u, FloatEq(0.0375)),
                           Pair(3u, FloatEq(0.3))));
}

TEST_F(HourBinPredictorTest, FromProtoDecays) {
  RecurrencePredictorProto proto = MakeTestingHourBinnedProto();
  proto.mutable_hour_bin_predictor()->set_last_decay_timestamp(350);
  predictor_->FromProto(proto);
  SetLocalTime(1, 11);
  EXPECT_THAT(predictor_->Rank(kCondition),
              UnorderedElementsAre(Pair(1u, FloatEq(0.15))));

  // Check if empty items got deleted during decay.
  EXPECT_EQ(
      static_cast<int>(predictor_->proto_.binned_frequency_table().size()), 1);
  EXPECT_EQ(static_cast<int>(
                (*predictor_->proto_.mutable_binned_frequency_table())[10]
                    .frequency()
                    .size()),
            1);
}

TEST_F(HourBinPredictorTest, ToProto) {
  RecurrencePredictorProto proto;
  SetLocalTime(1, 10);
  predictor_->Train(1u, kCondition);
  predictor_->Train(1u, kCondition);
  predictor_->Train(1u, kCondition);
  predictor_->Train(2u, kCondition);

  SetLocalTime(1, 11);
  predictor_->Train(1u, kCondition);
  predictor_->Train(3u, kCondition);
  predictor_->SetLastDecayTimestamp(365);

  predictor_->ToProto(&proto);
  RecurrencePredictorProto target_proto = MakeTestingHourBinnedProto();

  EXPECT_TRUE(proto.has_hour_bin_predictor());

  EXPECT_TRUE(EquivToProtoLite(proto.hour_bin_predictor(),
                               target_proto.hour_bin_predictor()));
}

class MarkovPredictorTest : public testing::Test {
 protected:
  void SetUp() override {
    predictor_ = std::make_unique<MarkovPredictor>(config_, "model");
  }

  MarkovPredictorConfig config_;
  std::unique_ptr<MarkovPredictor> predictor_;
};

TEST_F(MarkovPredictorTest, RankWithNoTargets) {
  // This should ignore the condition.
  EXPECT_TRUE(predictor_->Rank(kCondition).empty());
  EXPECT_TRUE(predictor_->Rank(1u).empty());
}

TEST_F(MarkovPredictorTest, RecordAndRank) {
  // Transitions 1 -> 2 -> 3. Condition should be ignored.
  predictor_->Train(1u, kCondition);
  predictor_->Train(2u, 2u);
  predictor_->Train(3u, 4u);
  predictor_->Train(1u, 6u);

  // Last target is 1, we've only seen 1 -> 2.
  EXPECT_THAT(predictor_->Rank(kCondition),
              UnorderedElementsAre(Pair(2u, FloatEq(1.0f))));

  predictor_->Train(3u, 8u);
  predictor_->Train(1u, 6u);
  predictor_->Train(1u, 4u);
  predictor_->Train(1u, 2u);

  // Last target is 1, now we've seen 1 -> {1, 2, 3}.
  EXPECT_THAT(
      predictor_->Rank(kCondition),
      UnorderedElementsAre(Pair(1u, FloatEq(0.5f)), Pair(2u, FloatEq(0.25f)),
                           Pair(3u, FloatEq(0.25f))));

  predictor_->Train(3u, 8u);
  predictor_->Train(3u, 8u);

  // Last target is 3, we have 3 -> {1, 3}.
  EXPECT_THAT(predictor_->Rank(kCondition),
              UnorderedElementsAre(Pair(1u, FloatEq(2.0f / 3.0f)),
                                   Pair(3u, FloatEq(1.0f / 3.0f))));
}

TEST_F(MarkovPredictorTest, Cleanup) {
  // 0 -> {1, 3} and all i -> {i+1}.
  for (int i = 0; i < 6; ++i)
    predictor_->Train(i, kCondition);
  predictor_->Train(0, kCondition);
  predictor_->Train(3, kCondition);

  predictor_->Cleanup({0u, 1u, 2u});

  // Expect 0 -> {1} with target 3 deleted.
  predictor_->previous_target_ = 0u;
  EXPECT_THAT(predictor_->Rank(0u),
              UnorderedElementsAre(Pair(1u, FloatEq(1.0f))));
  // Expect 1 -> {2} with nothing deleted.
  predictor_->previous_target_ = 1u;
  EXPECT_THAT(predictor_->Rank(1u),
              UnorderedElementsAre(Pair(2u, FloatEq(1.0f))));

  // Conditions 2, 3, 4, 5 should have been cleaned up. For 2, all targets are
  // deleted so the condition itself should be too. For the remainder, the
  // condition is invalid so should be deleted directly.
  for (int i = 3; i < 6; ++i) {
    predictor_->previous_target_ = i;
    EXPECT_TRUE(predictor_->Rank(kCondition).empty());
  }
}

TEST_F(MarkovPredictorTest, ToAndFromProto) {
  // Some complicated transitions.
  for (int i = 0; i < 10; ++i) {
    for (int j = 10; j < 10 + i; ++j) {
      for (int trains = 0; trains < j; ++trains) {
        predictor_->Train(i, kCondition);
        predictor_->Train(j, kCondition);
      }
    }
  }

  RecurrencePredictorProto proto;
  predictor_->ToProto(&proto);

  MarkovPredictor new_predictor(config_, "model");
  new_predictor.FromProto(proto);

  EXPECT_TRUE(proto.has_markov_predictor());
  for (int i = 0; i < 10; ++i) {
    // Set the last target without modifying the transition frequencies.
    predictor_->previous_target_ = i;
    new_predictor.previous_target_ = i;

    EXPECT_EQ(predictor_->Rank(5u), new_predictor.Rank(5u));
  }
}

class ExponentialWeightsEnsembleTest : public testing::Test {
 protected:
  // Test ensemble config with a fake, a frecency, and a conditional frequency
  // predictor.
  ExponentialWeightsEnsembleConfig MakeConfig() {
    ExponentialWeightsEnsembleConfig config;
    config.set_learning_rate(1.0f);

    config.add_predictors()->mutable_fake_predictor();
    config.add_predictors()->mutable_frecency_predictor()->set_decay_coeff(
        0.5f);
    config.add_predictors()->mutable_conditional_frequency_predictor();

    return config;
  }

  std::unique_ptr<ExponentialWeightsEnsemble> MakeEnsemble(
      const ExponentialWeightsEnsembleConfig& config) {
    return std::make_unique<ExponentialWeightsEnsemble>(config, "model");
  }

  // A predictor that always returns the same prediction, whose weight is
  // expected to decay in an ensemble.
  class BadPredictor : public FakePredictor {
   public:
    BadPredictor() : FakePredictor("model") {}

    // FakePredictor:
    std::map<unsigned int, float> Rank(unsigned int condition) override {
      return {{417u, 1.0f}};
    }
  };
};

TEST_F(ExponentialWeightsEnsembleTest, RankWithNoTargets) {
  auto ensemble = MakeEnsemble(MakeConfig());
  EXPECT_TRUE(ensemble->Rank(kCondition).empty());
}

TEST_F(ExponentialWeightsEnsembleTest, SimpleRecordAndRank) {
  // Test a model with a single predictor. Because there is only one model, its
  // weight should always be 1.0.
  ExponentialWeightsEnsembleConfig config;
  config.set_learning_rate(1.0f);
  config.add_predictors()->mutable_conditional_frequency_predictor();
  auto ewe = MakeEnsemble(config);

  ewe->Train(0u, 0u);
  ewe->Train(0u, 0u);
  ewe->Train(0u, 0u);
  ewe->Train(0u, 0u);
  ewe->Train(2u, 1u);
  ewe->Train(2u, 1u);
  ewe->Train(2u, 1u);
  ewe->Train(3u, 1u);

  EXPECT_THAT(ewe->Rank(0u), UnorderedElementsAre(Pair(0u, FloatEq(1.0f))));
  EXPECT_THAT(ewe->Rank(1u), UnorderedElementsAre(Pair(2u, FloatEq(0.75f)),
                                                  Pair(3u, FloatEq(0.25f))));
}

TEST_F(ExponentialWeightsEnsembleTest, GoodModelAndBadModel) {
  // Test with two predictors, one of which is always wrong and whose weight
  // should go to zero.
  ExponentialWeightsEnsembleConfig config;
  config.set_learning_rate(1.0f);
  config.add_predictors()->mutable_fake_predictor();
  // Because the bad predictor isn't a real predictor, add a fake predictor and
  // manually replace it after the ensemble is constructed.
  config.add_predictors()->mutable_fake_predictor();

  auto ewe = MakeEnsemble(config);
  ewe->predictors_[1].first = std::make_unique<BadPredictor>();

  for (int i = 0; i < 5; ++i)
    ewe->Train(1u, 0u);
  for (int i = 0; i < 5; ++i)
    ewe->Train(2u, 0u);

  // Expect the result scores from the ensemble to be approximately the scores
  // from the predictor itself, as the weight should be near 1. Expect the
  // result from the bad predictor to have a score near 0.
  EXPECT_THAT(ewe->predictors_[0].second, FloatNear(1.0f, 0.05f));
  EXPECT_THAT(ewe->predictors_[1].second, FloatNear(0.0f, 0.05f));
  EXPECT_THAT(ewe->Rank(0u),
              UnorderedElementsAre(Pair(1u, FloatNear(5.0f, 0.05f)),
                                   Pair(2u, FloatNear(5.0f, 0.05f)),
                                   Pair(417u, FloatNear(0.0f, 0.05f))));
}

TEST_F(ExponentialWeightsEnsembleTest, TwoBalancedModels) {
  // Test with two identical predictors. Their weights should stay balanced over
  // time.
  ExponentialWeightsEnsembleConfig config;
  config.set_learning_rate(1.0f);
  config.add_predictors()->mutable_fake_predictor();
  config.add_predictors()->mutable_fake_predictor();
  auto ewe = MakeEnsemble(config);

  for (int i = 0; i < 5; ++i)
    ewe->Train(1u, kCondition);
  for (int i = 0; i < 5; ++i)
    ewe->Train(2u, kCondition);

  // The scores should be exactly those from one fake predictor, and their
  // weights should be 0.5 each.
  EXPECT_THAT(ewe->predictors_[0].second, FloatEq(0.5f));
  EXPECT_THAT(ewe->predictors_[1].second, FloatEq(0.5f));
  EXPECT_THAT(
      ewe->Rank(kCondition),
      UnorderedElementsAre(Pair(1u, FloatEq(5.0f)), Pair(2u, FloatEq(5.0f))));
}

TEST_F(ExponentialWeightsEnsembleTest, ToAndFromProto) {
  // Add in another predictor for completeness.
  auto config = MakeConfig();
  config.add_predictors()->mutable_markov_predictor();
  auto ensemble_a = MakeEnsemble(config);

  // Do some training.
  for (int i = 0; i < 10; ++i)
    for (int j = 0; j < i; ++j)
      ensemble_a->Train(j, i);
  for (int i = 0; i < 10; ++i)
    for (int j = 0; j < i; ++j)
      ensemble_a->Train(2 * j, 0u);

  // Expect a new ensemble loaded from the old ensemble's state to have the same
  // rankings.
  RecurrencePredictorProto proto;
  ensemble_a->ToProto(&proto);

  auto ensemble_b = MakeEnsemble(config);
  ensemble_b->FromProto(proto);

  for (int i = 0; i < 10; ++i)
    EXPECT_EQ(ensemble_a->Rank(i), ensemble_b->Rank(i));
}

class FrequencyPredictorTest : public testing::Test {
 protected:
  void SetUp() override {
    predictor_ = std::make_unique<FrequencyPredictor>(config_, "model");
  }

  FrequencyPredictorConfig config_;
  std::unique_ptr<FrequencyPredictor> predictor_;
};

TEST_F(FrequencyPredictorTest, RankWithNoTargets) {
  EXPECT_TRUE(predictor_->Rank(kCondition).empty());
}

TEST_F(FrequencyPredictorTest, RecordAndRankSimple) {
  predictor_->Train(2u, kCondition);
  predictor_->Train(4u, kCondition);
  predictor_->Train(6u, kCondition);
  predictor_->Train(6u, kCondition);

  EXPECT_THAT(
      predictor_->Rank(kCondition),
      UnorderedElementsAre(Pair(2u, FloatEq(0.25f)), Pair(4u, FloatEq(0.25f)),
                           Pair(6u, FloatEq(0.5f))));
}

TEST_F(FrequencyPredictorTest, RecordAndRankComplex) {
  predictor_->Train(2u, kCondition);
  predictor_->Train(4u, kCondition);
  predictor_->Train(6u, kCondition);
  predictor_->Train(4u, kCondition);
  predictor_->Train(2u, kCondition);

  // Ranks should be deterministic.
  for (int i = 0; i < 3; ++i) {
    EXPECT_THAT(predictor_->Rank(kCondition),
                UnorderedElementsAre(Pair(2u, FloatEq(2.0f / 5.0f)),
                                     Pair(4u, FloatEq(2.0f / 5.0f)),
                                     Pair(6u, FloatEq(1.0f / 5.0f))));
  }
}

TEST_F(FrequencyPredictorTest, Cleanup) {
  for (int i = 0; i < 6; ++i)
    predictor_->Train(i, kCondition);
  predictor_->Cleanup({0u, 2u, 4u});

  EXPECT_THAT(predictor_->Rank(kCondition),
              UnorderedElementsAre(Pair(0u, _), Pair(2u, _), Pair(4u, _)));
}

TEST_F(FrequencyPredictorTest, ToAndFromProto) {
  predictor_->Train(1u, kCondition);
  predictor_->Train(3u, kCondition);
  predictor_->Train(5u, kCondition);

  RecurrencePredictorProto proto;
  predictor_->ToProto(&proto);

  FrequencyPredictor new_predictor(config_, "model");
  new_predictor.FromProto(proto);

  EXPECT_TRUE(proto.has_frequency_predictor());
  EXPECT_EQ(proto.frequency_predictor().counts_size(), 3);
  EXPECT_EQ(predictor_->Rank(kCondition), new_predictor.Rank(kCondition));
}

}  // namespace app_list
