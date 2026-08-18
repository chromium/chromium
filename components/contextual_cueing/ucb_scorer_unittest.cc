// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_cueing/ucb_scorer.h"

#include <cmath>

#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_cueing {

class UcbScorerTest : public testing::Test {
  // Default hyperparameters from design doc.
  const double c_ = std::sqrt(2.0);
  const double alpha_ = 1.0;
  const double beta_ = 0.5;
  const double gamma_ = 1.0;

 protected:
  UCBHyperparameters params_{.c = c_,
                             .alpha = alpha_,
                             .beta = beta_,
                             .gamma = gamma_,
                             .max_explore_impressions = 50};
};

// Tests that Laplace smoothing prevents divide-by-zero and initializes score.
TEST_F(UcbScorerTest, ColdStartZeroImpressions) {
  TargetStats stats{.impressions = 0, .clicks = 0, .dismissals = 0};
  int total_impressions = 0;

  double score = CalculateUCBScore(stats, total_impressions, params_);

  // Expected exploit: (0 - 0 + 0.5) / (0 + 1.0) = 0.5
  // Expected explore: c_ * sqrt(log(0 + 1.0) / (0 + 1.0)) = c_ * sqrt(0) = 0
  // Total expected: 0.5
  EXPECT_DOUBLE_EQ(score, 0.5);
}

// Tests that the exploration bonus decays as impressions accumulate.
TEST_F(UcbScorerTest, ExplorationDecay) {
  TargetStats stats{.impressions = 0, .clicks = 0, .dismissals = 0};

  // With total_impressions > 0, explore bonus should trigger.
  double score_1 = CalculateUCBScore(stats, 10, params_);
  double score_2 = CalculateUCBScore(stats, 100, params_);

  // A source with 0 impressions should get a higher score when the total system
  // impressions increase, because its relative uncertainty increases.
  EXPECT_GT(score_2, score_1);

  // Now, increase impressions for *this* source while keeping total constant.
  TargetStats stats_few{.impressions = 1, .clicks = 0, .dismissals = 0};
  TargetStats stats_many{.impressions = 10, .clicks = 0, .dismissals = 0};

  double score_few = CalculateUCBScore(stats_few, 100, params_);
  double score_many = CalculateUCBScore(stats_many, 100, params_);

  // The source with more impressions should have a smaller explore bonus.
  EXPECT_GT(score_few, score_many);
}

// Tests that clicks boost the score and dismissals penalize it.
TEST_F(UcbScorerTest, ExploitPreference) {
  int total_impressions = 100;

  // Arm A: clicked 10 times out of 20.
  TargetStats stats_clicked{.impressions = 20, .clicks = 10, .dismissals = 0};
  // Arm B: dismissed 10 times out of 20.
  TargetStats stats_dismissed{.impressions = 20, .clicks = 0, .dismissals = 10};

  double score_clicked =
      CalculateUCBScore(stats_clicked, total_impressions, params_);
  double score_dismissed =
      CalculateUCBScore(stats_dismissed, total_impressions, params_);

  // Equal impressions, so explore bonus is identical. Clicks must dominate.
  EXPECT_GT(score_clicked, score_dismissed);

  // Arm C: no actions out of 20 (ignored/timeout).
  TargetStats stats_ignored{.impressions = 20, .clicks = 0, .dismissals = 0};
  double score_ignored =
      CalculateUCBScore(stats_ignored, total_impressions, params_);

  // Clicks > Ignored > Dismissed
  EXPECT_GT(score_clicked, score_ignored);
  EXPECT_GT(score_ignored, score_dismissed);
}

// Tests that NaN is not generated during cold start when gamma is less
// than 1.0.
TEST_F(UcbScorerTest, ColdStartSmallGammaNaNPrevented) {
  TargetStats stats{.impressions = 0, .clicks = 0, .dismissals = 0};
  int total_impressions = 0;
  double small_gamma = 0.5;
  UCBHyperparameters params = params_;
  params.gamma = small_gamma;

  double score = CalculateUCBScore(stats, total_impressions, params);
  EXPECT_FALSE(std::isnan(score));
  EXPECT_GE(score, 0.0);
}

// Tests that explore bonus is capped when total impressions exceed
// max_explore_impressions.
TEST_F(UcbScorerTest, ExploreCapped) {
  TargetStats stats{.impressions = 0, .clicks = 0, .dismissals = 0};
  // Set cap to 50
  UCBHyperparameters params = params_;
  params.max_explore_impressions = 50;

  double score_at_cap = CalculateUCBScore(stats, 50, params);
  double score_above_cap = CalculateUCBScore(stats, 100, params);

  // Score should be identical because n_total is clamped to 50.
  EXPECT_DOUBLE_EQ(score_at_cap, score_above_cap);
}

// Tests that negative parameters are clamped to 0 (or safe minimum).
TEST_F(UcbScorerTest, NegativeParamsClamped) {
  TargetStats stats{.impressions = 10, .clicks = 5, .dismissals = 2};
  int total_impressions = 20;

  UCBHyperparameters negative_params;
  negative_params.c = -1.0;
  negative_params.alpha = -0.5;
  negative_params.beta = -0.2;
  negative_params.gamma = -1.0;
  negative_params.max_explore_impressions = -10;

  // This should compile and run without crashing, using clamped values:
  // c -> 0.0, alpha -> 0.0, beta -> 0.0, gamma -> 0.0 (safe_gamma -> 0.00001),
  // max_explore -> 0
  double score = CalculateUCBScore(stats, total_impressions, negative_params);

  // Expected exploit: (5 - 0 * 2 + 0) / (10 + 0.00001) = 5 / 10.00001 =~ 0.5
  // Expected explore: 0.0 * sqrt(...) = 0.0
  // Total expected: ~0.5
  EXPECT_NEAR(score, 0.5, 0.001);
}

// Tests that inconsistent stats are clamped to maintain mathematical validity.
TEST_F(UcbScorerTest, InconsistentStatsClamped) {
  // Negative stats should be clamped to 0.
  TargetStats negative_stats{
      .impressions = -10, .clicks = -5, .dismissals = -2};
  double score_neg = CalculateUCBScore(negative_stats, -20, params_);
  // Should evaluate as stats={0,0,0}, total_impressions=0.
  // exploit: (0 - 0 + 0.5) / (0 + 1.0) = 0.5
  // explore: 0.0
  EXPECT_DOUBLE_EQ(score_neg, 0.5);

  // clicks + dismissals (15) > impressions (10)
  // Should clamp impressions to 15.
  // total_impressions (10) < impressions (15)
  // Should clamp total_impressions to 15.
  TargetStats stats{.impressions = 10, .clicks = 10, .dismissals = 5};
  double score = CalculateUCBScore(stats, 10, params_);

  // Expected clamped stats: impressions = 15, clicks = 10, dismissals = 5.
  // Expected clamped total_impressions (n_total): 15.
  // exploit: (10 - 1.0 * 5 + 0.5) / (15 + 1.0) = 5.5 / 16.0 = 0.34375
  // explore: c_ * sqrt(log(max(15 + 1.0, 1.0)) / (15 + 1.0))
  //          = c_ * sqrt(log(16.0) / 16.0)
  //          = 1.41421356 * sqrt(2.7725887 / 16.0)
  //          = 1.41421356 * sqrt(0.17328679)
  //          = 1.41421356 * 0.4162773
  //          = 0.5886997
  // Total expected: 0.34375 + 0.5886997 = 0.9324497
  double expected_exploit = (10.0 - 1.0 * 5.0 + 0.5) / (15.0 + 1.0);
  double expected_explore = params_.c * std::sqrt(std::log(16.0) / 16.0);
  EXPECT_DOUBLE_EQ(score, expected_exploit + expected_explore);
}

}  // namespace contextual_cueing
