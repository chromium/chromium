// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_cueing/ucb_scorer.h"

#include <algorithm>
#include <cmath>

namespace contextual_cueing {

double CalculateUCBScore(const TargetStats& stats,
                         int total_impressions,
                         const UCBHyperparameters& params) {
  // Enforce safe gamma to prevent division by zero in release builds.
  double safe_gamma = std::max(params.gamma, 0.00001);

  // Guardrails: ensure parameters are non-negative and mathematically valid.
  double c = std::max(0.0, params.c);
  double alpha = std::max(0.0, params.alpha);
  double beta = std::max(0.0, params.beta);
  int max_explore_impressions = std::max(0, params.max_explore_impressions);

  // Validate stats are within expected bounds.
  double impressions = std::max(0.0, static_cast<double>(stats.impressions));
  double clicks = std::max(0.0, static_cast<double>(stats.clicks));
  double dismissals = std::max(0.0, static_cast<double>(stats.dismissals));
  double n_total = std::max(0.0, static_cast<double>(total_impressions));

  // Enforce mathematical consistency gracefully without crashing.
  impressions = std::max(impressions, clicks + dismissals);
  n_total = std::max(n_total, impressions);

  // Exploit component: adjusted engagement rate with Laplace smoothing.
  double exploit =
      (clicks - alpha * dismissals + beta) / (impressions + safe_gamma);

  // Explore component: uncertainty bonus. Clamped to limit maximum explore
  // bonus and prevent newly launched features from hijacking UI. Logarithm
  // input is clamped to 1.0 or greater to prevent NaN when total_impressions
  // + safe_gamma is less than 1.0.
  double n_total_clamped =
      std::min(n_total, static_cast<double>(max_explore_impressions));
  double explore =
      c * std::sqrt(std::log(std::max(n_total_clamped + safe_gamma, 1.0)) /
                    (impressions + safe_gamma));

  return exploit + explore;
}

}  // namespace contextual_cueing
