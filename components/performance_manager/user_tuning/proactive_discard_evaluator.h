// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_USER_TUNING_PROACTIVE_DISCARD_EVALUATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_USER_TUNING_PROACTIVE_DISCARD_EVALUATOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/performance_manager/public/decorators/tab_page_decorator.h"

namespace performance_manager {

// This component uses `RevisitProbabilityEstimator` to estimate the likelihood
// of a particular tab being revisited within a given timeframe. `Sampler`
// specializations can control when this evaluation takes place. If a tab is
// deemed unlikely to be revisited for a given sample, it will be considered
// eligible for discarding unless something else marks it as "protected".
class ProactiveDiscardEvaluator {
 public:
  class RevisitProbabilityEstimator {
   public:
    virtual ~RevisitProbabilityEstimator() = default;

    // Computes and returns the probability of `tab_handle` being revisited.
    virtual float ComputeRevisitProbability(
        const TabPageDecorator::TabHandle* tab_handle) = 0;
  };

  class Sampler {
   public:
    virtual ~Sampler() = default;

    void Attach(ProactiveDiscardEvaluator* evaluator) {
      CHECK(!evaluator_);
      evaluator_ = evaluator;
    }

   protected:
    // Triggers the evaluation of `tab_handle` for discard eligibility.
    void Sample(const TabPageDecorator::TabHandle* tab_handle);

   private:
    // `evaluator_` owns this sampler.
    raw_ptr<ProactiveDiscardEvaluator> evaluator_ = nullptr;
  };

  using DiscardFunction =
      base::RepeatingCallback<void(const TabPageDecorator::TabHandle*)>;

  ProactiveDiscardEvaluator(
      std::unique_ptr<RevisitProbabilityEstimator> estimator,
      std::unique_ptr<Sampler> sampler,
      DiscardFunction discard_function);
  ~ProactiveDiscardEvaluator();

  // Measures the likelihood of `tab_handle` being revisited, and attempts to
  // discard it if the probability is low enough. Returns true if an attempt is
  // made, regardless of the result.
  bool TryDiscard(const TabPageDecorator::TabHandle* tab_handle);

 private:
  std::unique_ptr<RevisitProbabilityEstimator> estimator_;
  std::unique_ptr<Sampler> sampler_;
  DiscardFunction discard_function_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_USER_TUNING_PROACTIVE_DISCARD_EVALUATOR_H_
