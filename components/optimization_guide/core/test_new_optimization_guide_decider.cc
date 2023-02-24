// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/test_new_optimization_guide_decider.h"

namespace optimization_guide {

TestNewOptimizationGuideDecider::TestNewOptimizationGuideDecider() = default;
TestNewOptimizationGuideDecider::~TestNewOptimizationGuideDecider() = default;

void TestNewOptimizationGuideDecider::RegisterOptimizationTypes(
    const std::vector<proto::OptimizationType>& optimization_types) {
  registered_optimization_types_.insert(registered_optimization_types_.end(),
                                        optimization_types.begin(),
                                        optimization_types.end());
}

void TestNewOptimizationGuideDecider::CanApplyOptimization(
    const GURL& url,
    proto::OptimizationType optimization_type,
    OptimizationGuideDecisionCallback callback) {
  std::move(callback).Run(OptimizationGuideDecision::kFalse,
                          /*optimization_metadata=*/{});
}

OptimizationGuideDecision TestNewOptimizationGuideDecider::CanApplyOptimization(
    const GURL& url,
    proto::OptimizationType optimization_type,
    OptimizationMetadata* optimization_metadata) {
  return OptimizationGuideDecision::kFalse;
}

void TestNewOptimizationGuideDecider::CanApplyOptimizationOnDemand(
    const std::vector<GURL>& urls,
    const base::flat_set<proto::OptimizationType>& optimization_types,
    proto::RequestContext request_context,
    OnDemandOptimizationGuideDecisionRepeatingCallback callback) {
  for (const auto& url : urls) {
    base::flat_map<proto::OptimizationType,
                   OptimizationGuideDecisionWithMetadata>
        decisions;
    for (const auto optimization_type : optimization_types) {
      decisions[optimization_type] = {OptimizationGuideDecision::kFalse,
                                      /*optimization_metadata=*/{}};
    }
    callback.Run(url, decisions);
  }
}

}  // namespace optimization_guide
