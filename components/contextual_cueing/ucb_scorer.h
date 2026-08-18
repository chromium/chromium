// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_CUEING_UCB_SCORER_H_
#define COMPONENTS_CONTEXTUAL_CUEING_UCB_SCORER_H_

namespace contextual_cueing {

// Interaction statistics for a candidate cue target.
struct TargetStats {
  int impressions = 0;
  int clicks = 0;
  int dismissals = 0;
};

// Hyperparameters for the UCB (Upper Confidence Bound) scoring function.
struct UCBHyperparameters {
  double c = 1.414;                  // Exploration weight.
  double alpha = 1.0;                // Penalty factor for dismissals.
  double beta = 0.5;                 // Click prior (Laplace smoothing).
  double gamma = 1.0;                // Impression prior (Laplace smoothing).
  int max_explore_impressions = 50;  // Cap for n_total to limit explore bonus.
};

// Calculates the UCB score for a given source candidate.
// Higher score indicates a higher rank.
//
// Formula:
// Score = (clicks - alpha * dismissals + beta) / (impressions + safe_gamma)
//         + c * sqrt(ln(max(min(n_total, max_explore) + safe_gamma, 1.0)) /
//                    (impressions + safe_gamma))
//
// `stats` holds personal interaction stats for the candidate being scored.
// `total_impressions` is the global count of impressions across all cue
// sources for this profile.
double CalculateUCBScore(const TargetStats& stats,
                         int total_impressions,
                         const UCBHyperparameters& params);

}  // namespace contextual_cueing

#endif  // COMPONENTS_CONTEXTUAL_CUEING_UCB_SCORER_H_
