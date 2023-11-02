// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_TEST_OPTIMIZATION_GUIDE_DECIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_TEST_OPTIMIZATION_GUIDE_DECIDER_H_

#include "components/optimization_guide/content/browser/optimization_guide_decider.h"

namespace optimization_guide {

// An implementation of |OptimizationGuideDecider| that can be selectively
// mocked out for unit testing features that rely on the Optimization Guide in
// //components/...
class TestOptimizationGuideDecider : public OptimizationGuideDecider {
 public:
  TestOptimizationGuideDecider();
  ~TestOptimizationGuideDecider() override;
  TestOptimizationGuideDecider(const TestOptimizationGuideDecider&) = delete;
  TestOptimizationGuideDecider& operator=(const TestOptimizationGuideDecider&) =
      delete;

  // OptimizationGuideDecider implementation:
  void RegisterOptimizationTypes(
      const std::vector<proto::OptimizationType>& optimization_types) override;
  void CanApplyOptimizationAsync(
      content::NavigationHandle* navigation_handle,
      proto::OptimizationType optimization_type,
      OptimizationGuideDecisionCallback callback) override;
  OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      proto::OptimizationType optimization_type,
      OptimizationMetadata* optimization_metadata) override;

 private:
  // OptimizationGuideDecider implementation:
  void CanApplyOptimizationOnDemand(
      const std::vector<GURL>& urls,
      const base::flat_set<proto::OptimizationType>& optimization_types,
      proto::RequestContext request_context,
      OnDemandOptimizationGuideDecisionRepeatingCallback callback) override;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_TEST_OPTIMIZATION_GUIDE_DECIDER_H_
