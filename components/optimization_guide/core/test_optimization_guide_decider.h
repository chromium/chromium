// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEST_OPTIMIZATION_GUIDE_DECIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEST_OPTIMIZATION_GUIDE_DECIDER_H_

#include "base/functional/callback.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace optimization_guide {

// Meant to be used in unit tests for services that use the decider.
class TestOptimizationGuideDecider : public OptimizationGuideDecider {
 public:
  TestOptimizationGuideDecider();
  ~TestOptimizationGuideDecider() override;

  const std::vector<proto::OptimizationType>& registered_optimization_types()
      const {
    return registered_optimization_types_;
  }

  // OptimizationGuideDecider:
  void RegisterOptimizationTypes(
      const std::vector<proto::OptimizationType>& optimization_types) override;
  void CanApplyOptimization(
      const GURL& url,
      proto::OptimizationType optimization_type,
      OptimizationGuideDecisionCallback callback) override;
  OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      proto::OptimizationType optimization_type,
      OptimizationMetadata* optimization_metadata) override;
  void CanApplyOptimizationOnDemand(
      const std::vector<GURL>& urls,
      const base::flat_set<proto::OptimizationType>& optimization_types,
      proto::RequestContext request_context,
      OnDemandOptimizationGuideDecisionRepeatingCallback callback,
      std::optional<proto::RequestContextMetadata> request_context_metadata =
          std::nullopt) override;

 private:
  // Stored calls to these methods, for testing usage.
  std::vector<proto::OptimizationType> registered_optimization_types_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEST_OPTIMIZATION_GUIDE_DECIDER_H_
