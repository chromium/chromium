// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/user_tuning/proactive_discard_evaluator.h"

#include "components/performance_manager/public/features.h"

namespace performance_manager {

void ProactiveDiscardEvaluator::Sampler::Sample(
    const TabPageDecorator::TabHandle* tab_handle) {
  CHECK(evaluator_);
  evaluator_->TryDiscard(tab_handle);
}

ProactiveDiscardEvaluator::ProactiveDiscardEvaluator(
    std::unique_ptr<RevisitProbabilityEstimator> estimator,
    std::unique_ptr<Sampler> sampler,
    DiscardFunction discard_function)
    : estimator_(std::move(estimator)),
      sampler_(std::move(sampler)),
      discard_function_(discard_function) {
  CHECK(estimator_);
  CHECK(discard_function_);
  sampler_->Attach(this);
}

ProactiveDiscardEvaluator::~ProactiveDiscardEvaluator() = default;

bool ProactiveDiscardEvaluator::TryDiscard(
    const TabPageDecorator::TabHandle* tab_handle) {
  static const float false_positive_target =
      static_cast<float>(
          features::kProactiveDiscardingTargetFalsePositivePercent.Get()) /
      100.0f;
  CHECK_GT(false_positive_target, 0.0f);

  float probability = estimator_->ComputeRevisitProbability(tab_handle);

  if (probability <= false_positive_target) {
    discard_function_.Run(tab_handle);

    return true;
  }

  return false;
}

}  // namespace performance_manager
